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
	incoming.ScheduleRead();

	/* wait for up to 5 seconds for login packets */
	timeout.Schedule(std::chrono::seconds{5});

	if (per_client != nullptr)
		per_client->AddConnection(accounting);
}

Connection::~Connection() noexcept
{
	outgoing.Close();
	incoming.Close();
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
		accounting.UpdateTokenBucket(10);
		Destroy();
		return;
	}

	const auto username = UO::ExtractString(packets.login.credentials.username);
	const auto password = UO::ExtractString(packets.login.credentials.password);
	if (!IsValidUsername(username)) {
		accounting.UpdateTokenBucket(8);
		Destroy();
		return;
	}

	try {
		if (!instance.GetDatabase().CheckCredentials(username, password)) {
			fmt::print(stderr, "Bad password for user {:?} from {}\n",
				   username, remote_address);

			accounting.UpdateTokenBucket(5);
			Destroy();
			return;
		}
	} catch (...) {
		PrintException(std::current_exception());
		accounting.UpdateTokenBucket(2);
		Destroy();
		return;
	}

	accounting.UpdateTokenBucket(1);
	fmt::print(stderr, "Accepted password for user {:?} from {}\n",
		   username, remote_address);

	/* connect to the actual game server */
	state = State::CONNECTING;
	connect.Connect(instance.GetConfig().game_server, std::chrono::seconds{10});
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
	}

	if (events & incoming.WRITE) {
		if (!DoSpliceSend(outgoing, incoming, splice_out_in)) {
			Destroy();
			return;
		}
	}

	if (events & incoming.READ) {
		switch (splice_in_out.ReceiveFrom(instance.GetPipeStock(), incoming.GetSocket())) {
		case Splice::ReceiveResult::OK:
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

void
Connection::OnOutgoingReady(unsigned events) noexcept
{
	assert(state == State::READY);
	assert(incoming.IsDefined());
	assert(!connect.IsPending());

	if (events & (outgoing.ERROR | outgoing.HANGUP)) {
		accounting.UpdateTokenBucket(5);
		Destroy();
		return;
	}

	if (events & outgoing.WRITE) {
		if (!DoSpliceSend(incoming, outgoing, splice_in_out)) {
			Destroy();
			return;
		}
	}

	if (events & outgoing.READ) {
		switch (splice_out_in.ReceiveFrom(instance.GetPipeStock(), outgoing.GetSocket())) {
		case Splice::ReceiveResult::OK:
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
				.length = remote_ip_buffer.length(),
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

	outgoing.Open(fd.Release());
	outgoing.ScheduleRead();
	incoming.ScheduleRead();
	state = State::READY;
}

void
Connection::OnSocketConnectError(std::exception_ptr e) noexcept
{
	assert(state == State::CONNECTING);

	fmt::print(stderr, "Failed to connect to {}: {}\n",
		   instance.GetConfig().game_server, std::move(e));

	Destroy();
}

