// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Database.hxx"
#include "PipeStock.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/net/PrometheusExporterHandler.hxx"
#include "net/ClientAccounting.hxx"
#include "net/SocketAddress.hxx"
#include "config.h"

#ifdef HAVE_LIBSYSTEMD
#include "event/systemd/Watchdog.hxx"
#endif // HAVE_LIBSYSTEMD

#include <forward_list>
#include <memory>

struct Config;
class Listener;
class KnockListener;
class PrometheusExporterListener;

class Instance final
	: PrometheusExporterHandler
{
	const Config &config;

	EventLoop event_loop;
	ShutdownListener shutdown_listener{event_loop, BIND_THIS_METHOD(OnShutdown)};

#ifdef HAVE_LIBSYSTEMD
	Systemd::Watchdog systemd_watchdog{event_loop};
#endif

	std::unique_ptr<PrometheusExporterListener> prometheus_exporter;

	PipeStock pipe_stock{event_loop};

	Database database;

	ClientAccountingMap client_accounting{event_loop, 16, true};

	std::forward_list<Listener> listeners;
	std::forward_list<KnockListener> knock_listeners;

public:
	struct {
		std::size_t client_connections, server_connections;

		uint_least64_t client_connections_accepted, server_connections_established, server_connections_failed;

		uint_least64_t accepted_knocks, rejected_knocks, malformed_knocks, missing_knocks;
		uint_least64_t accepted_logins, rejected_logins, malformed_logins;
		uint_least64_t delayed_connections;

		uint_least64_t client_bytes, server_bytes;
	} metrics{};

	[[nodiscard]]
	explicit Instance(const Config &_config);
	~Instance() noexcept;

	const Config &GetConfig() const noexcept {
		return config;
	}

	EventLoop &GetEventLoop() noexcept {
		return event_loop;
	}

	PipeStock &GetPipeStock() noexcept {
		return pipe_stock;
	}

	bool RequireKnock() const noexcept {
		return !knock_listeners.empty();
	}

	Database &GetDatabase() noexcept {
		return database;
	}

	[[gnu::pure]]
	PerClientAccounting *GetClientAccounting(SocketAddress address) noexcept {
		return client_accounting.Get(address);
	}

	void AddPrometheusExporter(UniqueSocketDescriptor &&socket) noexcept;
	void AddListener(UniqueSocketDescriptor &&fd) noexcept;
	void AddKnockListener(UniqueSocketDescriptor &&fd,
			      const char *nft_set) noexcept;

private:
	void OnShutdown() noexcept;

	/* virtual methods from class PrometheusExporterHandler */
	std::string OnPrometheusExporterRequest() override;
	void OnPrometheusExporterError(std::exception_ptr error) noexcept override;
};
