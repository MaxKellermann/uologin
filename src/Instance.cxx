// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Instance.hxx"
#include "Config.hxx"
#include "Listener.hxx"
#include "KnockListener.hxx"
#include "thread/Pool.hxx"
#include "event/net/PrometheusExporterListener.hxx"
#include "util/PrintException.hxx"

#include <fmt/core.h>

Instance::Instance(const Config &_config)
	:config(_config),
	 database(thread_pool_get_queue(event_loop),
		  config.user_database.empty() ? nullptr : config.user_database.c_str(),
		  config.auto_reload_user_database)
{
	shutdown_listener.Enable();
}

Instance::~Instance() noexcept
{
#ifndef NDEBUG
	listeners.clear();
	assert(metrics.client_connections == 0);
	assert(metrics.server_connections == 0);
#endif
}

void
Instance::AddPrometheusExporter(UniqueSocketDescriptor &&socket) noexcept
{
	assert(!prometheus_exporter);

	PrometheusExporterHandler &handler = *this;
	prometheus_exporter = std::make_unique<PrometheusExporterListener>(event_loop,
									   std::move(socket),
									   handler);
}

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

	thread_pool_stop();

#ifdef HAVE_LIBSYSTEMD
	systemd_watchdog.Disable();
#endif

	knock_listeners.clear();
	listeners.clear();
	prometheus_exporter.reset();

	client_accounting.Shutdown();

	thread_pool_join();
}

std::string
Instance::OnPrometheusExporterRequest()
{
	return fmt::format(R"(
# HELP uologin_client_connections Current number of connections from clients
# TYPE uologin_client_connections gauge

# HELP uologin_server_connections Current number of connections to servers
# TYPE uologin_server_connections gauge

# HELP uologin_client_connections_accepted Counter for connections accepted from clients
# TYPE uologin_client_connections_accepted counter

# HELP uologin_server_connections_established Counter for connections established to servers
# TYPE uologin_server_connections_established counter

# HELP uologin_server_connections_failed Counter for failures to connect to servers
# TYPE uologin_server_connections_failed counter

# HELP uologin_accepted_knocks Counter for accepted UDP knocks
# TYPE uologin_accepted_knocks counter

# HELP uologin_rejected_knocks Counter for rejected UDP knocks
# TYPE uologin_rejected_knocks counter

# HELP uologin_missing_knocks Counter for TCP connections rejected due to missing UDP knock
# TYPE uologin_missing_knocks counter

# HELP uologin_malformed_knocks Counter for malformed UDP knock packets
# TYPE uologin_malformed_knocks counter

# HELP uologin_accepted_logins Counter for accepted logins
# TYPE uologin_accepted_logins counter

# HELP uologin_rejected_logins Counter for rejected logins
# TYPE uologin_rejected_logins counter

# HELP uologin_malformed_logins Counter for malformed logins
# TYPE uologin_malformed_logins counter

# HELP uologin_delayed_connections Counter for delayed connections
# TYPE uologin_delayed_connections counter

# HELP uologin_client_bytes Counter for bytes forwarded from clients to servers
# TYPE uologin_client_bytes counter

# HELP uologin_server_bytes Counter for bytes forwarded from servers to clients
# TYPE uologin_server_bytes counter

uologin_client_connections {}
uologin_server_connections {}

uologin_client_connections_accepted {}
uologin_server_connections_established {}
uologin_server_connections_failed {}

uologin_accepted_knocks {}
uologin_rejected_knocks {}
uologin_missing_knocks {}
uologin_malformed_knocks {}
uologin_accepted_logins {}
uologin_rejected_logins {}
uologin_malformed_logins {}
uologin_delayed_connections {}

uologin_client_bytes {}
uologin_server_bytes {}
)",
			   metrics.client_connections, metrics.server_connections,
			   metrics.client_connections_accepted,
			   metrics.server_connections_established,
			   metrics.server_connections_failed,

			   metrics.accepted_knocks,
			   metrics.rejected_knocks,
			   metrics.missing_knocks,
			   metrics.malformed_knocks,
			   metrics.accepted_logins,
			   metrics.rejected_logins,
			   metrics.malformed_logins,
			   metrics.delayed_connections,
			   metrics.client_bytes, metrics.server_bytes);
}

void
Instance::OnPrometheusExporterError(std::exception_ptr error) noexcept
{
	PrintException(std::move(error));
}
