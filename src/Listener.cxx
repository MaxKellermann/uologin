// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Listener.hxx"
#include "Instance.hxx"
#include "Connection.hxx"
#include "DelayedConnection.hxx"
#include "lib/fmt/SocketAddressFormatter.hxx"
#include "time/Cast.hxx"
#include "util/PrintException.hxx"

Listener::Listener(Instance &_instance, UniqueSocketDescriptor &&socket)
	:ServerSocket(_instance.GetEventLoop(), std::move(socket)),
	 instance(_instance)
{
}

Listener::~Listener() noexcept
{
	delayed_connections.clear_and_dispose(DeleteDisposer{});
	connections.clear_and_dispose(DeleteDisposer{});
}

void
Listener::AddConnection(PerClientAccounting *per_client,
			UniqueSocketDescriptor &&connection_fd,
			SocketAddress peer_address) noexcept
{
	auto *c = new Connection(instance, per_client,
				 std::move(connection_fd), peer_address);
	connections.push_front(*c);
}

void
Listener::OnAccept(UniqueSocketDescriptor connection_fd,
		   SocketAddress peer_address) noexcept
{
	PerClientAccounting *const per_client = instance.GetClientAccounting(peer_address);
	if (per_client != nullptr) {
		per_client->UpdateTokenBucket(1);

		if (instance.RequireKnock() && !per_client->HasKnocked()) {
			// TODO remove this log message (no spam)
			fmt::print(stderr, "Client {} has not knocked\n", peer_address);
			++instance.metrics.missing_knocks;
			return;
		}

		if (!per_client->Check()) {
			/* too many connections from this IP address -
			   reject the new connection */
			fmt::print(stderr, "Too many connections from {}\n", peer_address);

			// TODO send AccountLoginReject?
			return;
		}

		if (const auto delay = per_client->GetDelay(); delay.count() > 0) {
			fmt::print(stderr, "Connect from {} tarpit {}s\n", peer_address, ToFloatSeconds(delay));
			++instance.metrics.delayed_connections;
			auto *c = new DelayedConnection(instance, *this,
							*per_client, delay,
							std::move(connection_fd), peer_address);
			delayed_connections.push_back(*c);
			return;
		}
	}

	AddConnection(per_client, std::move(connection_fd), peer_address);
}

void
Listener::OnAcceptError(std::exception_ptr error) noexcept
{
	PrintException(std::move(error));
}
