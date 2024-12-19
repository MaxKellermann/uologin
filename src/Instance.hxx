// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "PipeStock.hxx"
#include "Listener.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
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

	void AddListener(UniqueSocketDescriptor &&fd) noexcept {
		listeners.emplace_front(event_loop, *this);
		listeners.front().Listen(std::move(fd));
	}

#ifdef HAVE_LIBSYSTEMD
	/**
	 * Listen for incoming connections on sockets passed by systemd
	 * (systemd socket activation).
	 */
	bool AddSystemdListener();
#endif // HAVE_LIBSYSTEMD

private:
	void OnShutdown() noexcept;
};
