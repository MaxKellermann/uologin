// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Database.hxx"
#include "PipeStock.hxx"
#include "Listener.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "net/ClientAccounting.hxx"
#include "net/SocketAddress.hxx"
#include "config.h"

#ifdef HAVE_LIBSYSTEMD
#include "event/systemd/Watchdog.hxx"
#endif // HAVE_LIBSYSTEMD

#include <forward_list>

class Instance {
	EventLoop event_loop;
	ShutdownListener shutdown_listener{event_loop, BIND_THIS_METHOD(OnShutdown)};

#ifdef HAVE_LIBSYSTEMD
	Systemd::Watchdog systemd_watchdog{event_loop};
#endif

	PipeStock pipe_stock{event_loop};

	Database database;

	ClientAccountingMap client_accounting{event_loop, 16, true};

	std::forward_list<Listener> listeners;

	const SocketAddress server_address;

public:
	explicit Instance(SocketAddress _server_address);

	EventLoop &GetEventLoop() noexcept {
		return event_loop;
	}

	PipeStock &GetPipeStock() noexcept {
		return pipe_stock;
	}

	const SocketAddress GetServerAddress() const noexcept {
		return server_address;
	}

	const Database &GetDatabase() const noexcept {
		return database;
	}

	[[gnu::pure]]
	PerClientAccounting *GetClientAccounting(SocketAddress address) noexcept {
		return client_accounting.Get(address);
	}

	void AddListener(UniqueSocketDescriptor &&fd) noexcept {
		listeners.emplace_front(*this, std::move(fd));
	}

private:
	void OnShutdown() noexcept;
};
