// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "net/SocketConfig.hxx"

struct Config {
	SocketConfig listener{
		.listen = 1024,
		.tcp_defer_accept = 10,
		.tcp_user_timeout = 20000,
		.tcp_no_delay = true,
	};

	SocketConfig knock_listener;

	AllocatedSocketAddress game_server;
};

Config
LoadConfigFile(const char *path);
