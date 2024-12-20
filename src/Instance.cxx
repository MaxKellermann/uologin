// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Instance.hxx"
#include "system/Error.hxx"

#include <stdexcept>

Instance::Instance(SocketAddress _server_address)
	:server_address(_server_address)
{
	shutdown_listener.Enable();
}

void
Instance::OnShutdown() noexcept
{
	shutdown_listener.Disable();

#ifdef HAVE_LIBSYSTEMD
	systemd_watchdog.Disable();
#endif

	listeners.clear();
}
