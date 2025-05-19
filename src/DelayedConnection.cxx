// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#include "DelayedConnection.hxx"
#include "Connection.hxx"
#include "Instance.hxx"
#include "Listener.hxx"

using std::string_view_literals::operator""sv;

DelayedConnection::DelayedConnection(Instance &_instance, Listener &_listener,
				     PerClientAccounting &per_client,
				     Event::Duration delay,
				     UniqueSocketDescriptor fd,
				     SocketAddress _peer_address) noexcept
	:listener(_listener),
	 peer_address(_peer_address),
	 timer(_instance.GetEventLoop(), BIND_THIS_METHOD(OnTimer)),
	 socket(_instance.GetEventLoop(), BIND_THIS_METHOD(OnSocketReady), fd.Release())
{
	per_client.AddConnection(accounting);

	timer.Schedule(delay);

	/* schedule just EPOLLRDHUP because it can reliably detect
	   hangups without having to poll for EPOLLIN */
	socket.Schedule(socket.READ_HANGUP);
}

DelayedConnection::~DelayedConnection() noexcept
{
	socket.Close();
}

void
DelayedConnection::OnTimer() noexcept
{
	UniqueSocketDescriptor fd{AdoptTag{}, socket.ReleaseSocket()};

	listener.AddConnection(accounting.GetPerClient(),
			       std::move(fd), peer_address);
	Destroy();
}

void
DelayedConnection::OnSocketReady([[maybe_unused]] unsigned events) noexcept
{
	/* client has disconnected */

	accounting.UpdateTokenBucket(4);
	Destroy();
}
