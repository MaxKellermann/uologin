// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Instance.hxx"
#include "Listener.hxx"
#include "KnockListener.hxx"

Instance::Instance(const char *user_database,
		   SocketAddress _server_address, bool _send_remote_ip)
	:database(user_database),
	 server_address(_server_address),
	 send_remote_ip(_send_remote_ip)
{
	shutdown_listener.Enable();
}

Instance::~Instance() noexcept = default;

void
Instance::AddListener(UniqueSocketDescriptor &&fd) noexcept
{
	listeners.emplace_front(*this, std::move(fd));
}

void
Instance::AddKnockListener(UniqueSocketDescriptor &&fd,
			   const char *nft_set) noexcept
{
	knock_listeners.emplace_front(*this, std::move(fd), nft_set);
}

void
Instance::OnShutdown() noexcept
{
	shutdown_listener.Disable();

#ifdef HAVE_LIBSYSTEMD
	systemd_watchdog.Disable();
#endif

	knock_listeners.clear();
	listeners.clear();

	client_accounting.Shutdown();
}
