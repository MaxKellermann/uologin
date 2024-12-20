// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Connection.hxx"
#include "Instance.hxx"
#include "Validate.hxx"
#include "uo/Command.hxx"
#include "uo/Packets.hxx"
#include "uo/String.hxx"

#include <fmt/core.h>

#include <span>
#include <string_view>

Connection::Connection(Instance &_instance,
		       UniqueSocketDescriptor &&_fd,
		       [[maybe_unused]] SocketAddress address)
	:instance(_instance),
	 incoming(instance.GetEventLoop(), BIND_THIS_METHOD(OnIncomingReady),
		  _fd.Release()),
	 outgoing(instance.GetEventLoop(), BIND_THIS_METHOD(OnOutgoingReady)),
	 connect(instance.GetEventLoop(), *this),
	 timeout(instance.GetEventLoop(), BIND_THIS_METHOD(OnTimeout))
{
	incoming.ScheduleRead();

	/* wait for up to 5 seconds for login packets */
	timeout.Schedule(std::chrono::seconds{5});
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

void
Connection::OnIncomingReady(unsigned events) noexcept
{
	if (events & (incoming.ERROR | incoming.HANGUP)) {
		Destroy();
		return;
	}

	assert(!connect.IsPending());

	if (!outgoing.IsDefined()) {
		/* read the initial packets (SEED and ACCOUNT_LOGIN)
		   from the socket */
		assert(events & incoming.READ);
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
			// TODO
			Destroy();
			return;
		}

		const auto username = UO::ExtractString(packets.login.credentials.username);
		const auto password = UO::ExtractString(packets.login.credentials.password);
		if (!IsValidUsername(username)) {
			// TODO
			Destroy();
			return;
		}

		if (!instance.GetDatabase().CheckCredentials(username, password)) {
			// TODO
			Destroy();
			return;
		}

		fmt::print(stderr, "username={:?}\n", username);

		// TODO verify credentials

		/* connect to the actual game server */
		connect.Connect(instance.GetServerAddress(), std::chrono::seconds{10});
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
	assert(incoming.IsDefined());
	assert(!connect.IsPending());

	if (events & (outgoing.ERROR | outgoing.HANGUP)) {
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
	Destroy();
}

void
Connection::OnSocketConnectSuccess(UniqueSocketDescriptor fd) noexcept
{
	assert(initial_packets_fill == initial_packets.size());

	if (const auto nbytes = fd.WriteNoWait(initial_packets); nbytes < 0) {
		// TODO log error?
		Destroy();
		return;
	} else if (static_cast<std::size_t>(nbytes) < initial_packets.size()) {
		// TODO log error?
		Destroy();
		return;
	}

	outgoing.Open(fd.Release());
	outgoing.ScheduleRead();
	incoming.ScheduleRead();
}

void
Connection::OnSocketConnectError(std::exception_ptr e) noexcept
{
	(void)e; // TODO
	Destroy();
}

