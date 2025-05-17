// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Connection.hxx"
#include "Config.hxx"
#include "Instance.hxx"
#include "Validate.hxx"
#include "uo/Command.hxx"
#include "uo/Packets.hxx"
#include "uo/String.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "lib/fmt/SocketAddressFormatter.hxx"
#include "net/ToString.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "io/Iovec.hxx"
#include "util/PrintException.hxx"
#include "util/ScopeExit.hxx"
#include "util/SpanCast.hxx"

#include <fmt/core.h>

#include <span>
#include <string_view>

Connection::Connection(Instance &_instance,
		       PerClientAccounting *per_client,
		       UniqueSocketDescriptor &&_fd,
		       SocketAddress address) noexcept
	:instance(_instance),
	 remote_address(address),
	 incoming(instance.GetEventLoop(), BIND_THIS_METHOD(OnIncomingReady),
		  _fd.Release()),
	 outgoing(instance.GetEventLoop(), BIND_THIS_METHOD(OnOutgoingReady)),
	 connect(instance.GetEventLoop(), *this),
	 timeout(instance.GetEventLoop(), BIND_THIS_METHOD(OnTimeout))
{
	++instance.metrics.client_connections;
	++instance.metrics.client_connections_accepted;

	incoming.ScheduleRead();

	/* wait for up to 5 seconds for login packets */
	timeout.Schedule(std::chrono::seconds{5});

	if (per_client != nullptr)
		per_client->AddConnection(accounting);
}

Connection::~Connection() noexcept
{
	if (cancel_ptr)
		cancel_ptr.Cancel();

	if (outgoing.IsDefined()) {
		outgoing.Close();
		--instance.metrics.server_connections;
	}

	incoming.Close();
	--instance.metrics.client_connections;
}

struct ExpectedPackets {
	struct uo_packet_seed seed;
	struct uo_packet_account_login login;
};

static bool
DoSpliceSend(SocketEvent &from, SocketEvent &to, Splice &s)
{
	switch (s.SendTo(to.GetSocket())) {
	case Splice::SendResult::OK:
		to.CancelWrite();
		break;

	case Splice::SendResult::PARTIAL:
	case Splice::SendResult::SOCKET_BLOCKING:
		from.CancelRead();
		to.ScheduleWrite();
		break;

	case Splice::SendResult::ERROR:
		// TODO errno
		return false;
	}

	return true;
}

inline bool
Connection::SendAccountLoginReject() noexcept
{
	static constexpr struct uo_packet_account_login_reject packet = {
		.cmd = UO::Command::AccountLoginReject,
		.reason = 0x03, // bad password
	};

	return incoming.GetSocket().Send(ReferenceAsBytes(packet), MSG_DONTWAIT) >= 0;
}

inline void
Connection::SendServerList() noexcept
{
	assert(initial_packets_fill == initial_packets.size());

	const auto &server_list = instance.GetConfig().server_list;
	assert(!server_list.empty());

	const std::size_t size = sizeof(struct uo_packet_server_list) +
		(server_list.size() - 1) * sizeof(struct uo_fragment_server_info);
	auto &packet = *reinterpret_cast<struct uo_packet_server_list *>(malloc(size));
	AtScopeExit(&packet) { free(&packet); };

	memset(&packet, 0, size);

	packet.cmd = UO::Command::ServerList;
	packet.length = size;
	packet.unknown_0x5d = 0x5d;
	packet.num_game_servers = server_list.size();

	for (unsigned i = 0; i < server_list.size(); ++i) {
		const auto &src = server_list[i];
		auto &dst = packet.game_servers[i];

		dst.index = i;
		snprintf(dst.name, sizeof(dst.name), "%s", src.name.c_str());
		dst.address = 0xdeadbeef;
	}

	if (incoming.GetSocket().Send(std::span{reinterpret_cast<std::byte *>(&packet), size}, MSG_DONTWAIT) < 0) {
		Destroy();
		return;
	}

	state = State::SERVER_LIST;
	incoming.ScheduleRead();
	timeout.Schedule(std::chrono::minutes{1});
}

inline void
Connection::ReceivePlayServer() noexcept
{
	assert(state == State::SERVER_LIST);
	assert(!instance.GetConfig().server_list.empty());
	assert(initial_packets_fill == initial_packets.size());

	struct uo_packet_play_server packet;

	const auto nbytes = incoming.GetSocket().ReadNoWait(ReferenceAsWritableBytes(packet));
	if (nbytes <= 0 || static_cast<std::size_t>(nbytes) != sizeof(packet) ||
	    packet.cmd != UO::Command::PlayServer ||
	    packet.index >= instance.GetConfig().server_list.size()) [[unlikely]] {
		Destroy();
		return;
	}

	outgoing_address = instance.GetConfig().server_list[packet.index].address;

	incoming.CancelRead();
	timeout.Cancel();

	/* connect to the actual game server */
	state = State::CONNECTING;
	send_play_server = true;
	connect.Connect(outgoing_address, std::chrono::seconds{10});
}

inline void
Connection::ReceiveLoginPackets() noexcept
{
	/* read the initial packets (SEED and ACCOUNT_LOGIN) from the
	   socket */

	assert(state == State::INITIAL);
	assert(initial_packets_fill < initial_packets.size());

	const auto nbytes = incoming.GetSocket().ReadNoWait(std::span{initial_packets}.subspan(initial_packets_fill));
	if (nbytes <= 0) [[unlikely]] {
		// TODO log error?
		Destroy();
		return;
	}

	initial_packets_fill += nbytes;

	if (initial_packets_fill < initial_packets.size())
		return;

	incoming.CancelRead();
	timeout.Cancel();

	const auto &packets = *reinterpret_cast<const ExpectedPackets *>(initial_packets.data());
	static_assert(sizeof(initial_packets) == sizeof(packets));

	if (packets.seed.cmd != UO::Command::Seed ||
	    packets.login.cmd != UO::Command::AccountLogin) {
		++instance.metrics.malformed_logins;
		accounting.UpdateTokenBucket(10);
		Destroy();
		return;
	}

	const auto username = UO::ExtractString(packets.login.credentials.username);
	const auto password = UO::ExtractString(packets.login.credentials.password);
	if (!IsValidUsername(username)) {
		++instance.metrics.malformed_logins;
		accounting.UpdateTokenBucket(8);

		if (SendAccountLoginReject())
			incoming.GetSocket().ShutdownWrite();
		Destroy();
		return;
	}

	state = State::CHECK_CREDENTIALS;
	incoming.CancelRead();

	try {
		instance.GetDatabase().CheckCredentials(username, password,
							BIND_THIS_METHOD(OnCheckCredentials), cancel_ptr);
	} catch (...) {
		PrintException(std::current_exception());
		accounting.UpdateTokenBucket(2);

		if (SendAccountLoginReject())
			incoming.GetSocket().ShutdownWrite();
		Destroy();
		return;
	}
}

inline void
Connection::OnCheckCredentials(std::string_view username, bool result) noexcept
{
	assert(state == State::CHECK_CREDENTIALS);

	cancel_ptr = {};

	if (!result) {
		fmt::print(stderr, "Bad password for user {:?} from {}\n",
			   username, remote_address);
		++instance.metrics.rejected_logins;

		accounting.UpdateTokenBucket(5);

		if (SendAccountLoginReject())
			incoming.GetSocket().ShutdownWrite();
		Destroy();
		return;
	}

	accounting.UpdateTokenBucket(1);
	fmt::print(stderr, "Accepted password for user {:?} from {}\n",
		   username, remote_address);
	++instance.metrics.accepted_logins;

	if (!instance.GetConfig().server_list.empty()) {
		SendServerList();
		return;
	}

	/* connect to the actual game server */
	state = State::CONNECTING;
	incoming.ScheduleRead();
	outgoing_address = instance.GetConfig().game_server;
	connect.Connect(outgoing_address, std::chrono::seconds{10});
}

void
Connection::OnIncomingReady(unsigned events) noexcept
{
	if (events & (incoming.ERROR | incoming.HANGUP)) {
		if (state == State::INITIAL)
			accounting.UpdateTokenBucket(4);

		Destroy();
		return;
	}

	assert(!connect.IsPending());

	if (state == State::INITIAL) {
		/* read the initial packets (SEED and ACCOUNT_LOGIN)
		   from the socket */
		assert(events & incoming.READ);
		ReceiveLoginPackets();
		return;
	} else if (state == State::SERVER_LIST) {
		assert(events & incoming.READ);
		ReceivePlayServer();
		return;
	}

	if (events & incoming.WRITE) {
		if (!DoSpliceSend(outgoing, incoming, splice_out_in)) {
			Destroy();
			return;
		}
	}

	if (events & incoming.READ) {
		splice_in_out.received_bytes = 0;
		switch (splice_in_out.ReceiveFrom(instance.GetPipeStock(), incoming.GetSocket())) {
		case Splice::ReceiveResult::OK:
			instance.metrics.client_bytes += splice_in_out.received_bytes;

			if (!DoSpliceSend(incoming, outgoing, splice_in_out)) {
				Destroy();
				return;
			}

			break;

		case Splice::ReceiveResult::SOCKET_BLOCKING:
			break;

		case Splice::ReceiveResult::SOCKET_CLOSED:
			/* close connection with FIN, not RST */
			outgoing.GetSocket().ShutdownWrite();

			Destroy();
			return;

		case Splice::ReceiveResult::PIPE_FULL:
			assert(outgoing.IsWritePending());
			incoming.CancelRead();
			return;

		case Splice::ReceiveResult::ERROR:
			// TODO errno
			Destroy();
			return;
		}
	}
}

inline bool
Discard(SocketDescriptor socket, std::size_t n) noexcept
{
	while (n > 0) {
		std::byte buffer[1024];

		auto nbytes = socket.ReadNoWait({buffer, std::min(sizeof(buffer), n)});
		if (nbytes <= 0)
			return false;

		n -= nbytes;
	}

	return true;
}

inline void
Connection::ReceiveServerList() noexcept
{
	UO::Command cmd;

	if (outgoing.GetSocket().ReadNoWait(ReferenceAsWritableBytes(cmd)) != sizeof(cmd) ||
		cmd != UO::Command::ServerList) {
		Destroy();
		return;
	}

	PackedBE16 length;

	if (outgoing.GetSocket().ReadNoWait(ReferenceAsWritableBytes(length)) != sizeof(length) ||
	    length < sizeof(struct uo_packet_server_list)) {
		Destroy();
		return;
	}

	if (!Discard(outgoing.GetSocket(), length - 3)) {
		Destroy();
		return;
	}

	static constexpr struct uo_packet_play_server play_server{
		.cmd = UO::Command::PlayServer,
		.index = 0,
	};

	if (outgoing.GetSocket().Send(ReferenceAsBytes(play_server), MSG_DONTWAIT) < 0) {
		Destroy();
		return;
	}

	incoming.ScheduleRead();
	state = State::READY;
}

void
Connection::OnOutgoingReady(unsigned events) noexcept
{
	assert(incoming.IsDefined());
	assert(!connect.IsPending());

	if (events & (outgoing.ERROR | outgoing.HANGUP)) {
		accounting.UpdateTokenBucket(5);
		Destroy();
		return;
	}

	if (state == State::SEND_PLAY_SERVER) {
		ReceiveServerList();
		return;
	}

	assert(state == State::READY);

	if (events & outgoing.WRITE) {
		if (!DoSpliceSend(incoming, outgoing, splice_in_out)) {
			Destroy();
			return;
		}
	}

	if (events & outgoing.READ) {
		splice_out_in.received_bytes = 0;
		switch (splice_out_in.ReceiveFrom(instance.GetPipeStock(), outgoing.GetSocket())) {
		case Splice::ReceiveResult::OK:
			instance.metrics.server_bytes += splice_out_in.received_bytes;

			if (!DoSpliceSend(outgoing, incoming, splice_out_in)) {
				Destroy();
				return;
			}

			break;

		case Splice::ReceiveResult::SOCKET_BLOCKING:
			break;

		case Splice::ReceiveResult::SOCKET_CLOSED:
			/* close connection with FIN, not RST */
			incoming.GetSocket().ShutdownWrite();

			Destroy();
			return;

		case Splice::ReceiveResult::PIPE_FULL:
			assert(incoming.IsWritePending());
			outgoing.CancelRead();
			return;

		case Splice::ReceiveResult::ERROR:
			// TODO errno
			Destroy();
			return;
		}
	}
}

void
Connection::OnTimeout() noexcept
{
	accounting.UpdateTokenBucket(7);
	Destroy();
}

inline bool
Connection::SendInitialPackets(SocketDescriptor socket) noexcept
{
	assert(initial_packets_fill == initial_packets.size());

	const auto &packets = *reinterpret_cast<const ExpectedPackets *>(initial_packets.data());
	static_assert(sizeof(initial_packets) == sizeof(packets));

	std::array<struct iovec, 5> v;
	std::size_t n = 0;

	v[n++] = MakeIovec(ReferenceAsBytes(packets.seed));

	struct uo_packet_extended remote_ip_header;
	std::string remote_ip_buffer;
	if (instance.GetConfig().send_remote_ip) {
		if (const auto remote_ip = HostToString(remote_address); !remote_ip.empty()) {
			remote_ip_buffer = fmt::format("REMOTE_IP={}", remote_ip);
			remote_ip_header = {
				.cmd = UO::Command::Extended,
				.length = sizeof(remote_ip_header) + remote_ip_buffer.length(),
				.extended_cmd = 0x5a6a,
			};

			v[n++] = MakeIovec(ReferenceAsBytes(remote_ip_header));
			v[n++] = MakeIovec(AsBytes(remote_ip_buffer));
		}
	}

	v[n++] = MakeIovec(ReferenceAsBytes(packets.login));

	return socket.Send(std::span{v}.first(n), MSG_DONTWAIT) > 0;
}

void
Connection::OnSocketConnectSuccess(UniqueSocketDescriptor fd) noexcept
{
	assert(state == State::CONNECTING);

	if (!SendInitialPackets(fd)) {
		// TODO log error?
		Destroy();
		return;
	}

	++instance.metrics.server_connections;
	++instance.metrics.server_connections_established;

	outgoing.Open(fd.Release());
	outgoing.ScheduleRead();

	if (send_play_server) {
		state = State::SEND_PLAY_SERVER;
	} else {
		incoming.ScheduleRead();
		state = State::READY;
	}
}

void
Connection::OnSocketConnectError(std::exception_ptr e) noexcept
{
	assert(state == State::CONNECTING);

	++instance.metrics.server_connections_failed;

	fmt::print(stderr, "Failed to connect to {}: {}\n",
		   outgoing_address, std::move(e));

	Destroy();
}

