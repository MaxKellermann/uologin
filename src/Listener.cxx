// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Listener.hxx"
#include "Instance.hxx"
#include "Connection.hxx"
#include "util/PrintException.hxx"

Listener::Listener(Instance &_instance, UniqueSocketDescriptor &&socket)
	:ServerSocket(_instance.GetEventLoop(), std::move(socket)),
	 instance(_instance)
{
}

Listener::~Listener() noexcept
{
	connections.clear_and_dispose(DeleteDisposer{});
}

void
Listener::OnAccept(UniqueSocketDescriptor connection_fd,
		   SocketAddress peer_address) noexcept
{
	auto *c = new Connection(instance, std::move(connection_fd), peer_address);
	connections.push_front(*c);
}

void
Listener::OnAcceptError(std::exception_ptr error) noexcept
{
	PrintException(std::move(error));
}
