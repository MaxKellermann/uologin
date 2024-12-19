// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Instance.hxx"
#include "system/Error.hxx"

#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include <stdexcept>

Instance::Instance(SocketAddress _server_address)
	:server_address(_server_address)
{
	shutdown_listener.Enable();
}

#ifdef HAVE_LIBSYSTEMD

bool
Instance::AddSystemdListener()
{
	int n = sd_listen_fds(true);
	if (n < 0)
		throw MakeErrno("sd_listen_fds() failed");

	if (n == 0)
		return false;

	for (int i = 0; i < n; ++i)
		AddListener(UniqueSocketDescriptor{SD_LISTEN_FDS_START + i});

	return true;
}

#endif // HAVE_LIBSYSTEMD

void
Instance::OnShutdown() noexcept
{
	shutdown_listener.Disable();

#ifdef HAVE_LIBSYSTEMD
	systemd_watchdog.Disable();
#endif

	listeners.clear();
}
