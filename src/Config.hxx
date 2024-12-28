// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "net/SocketConfig.hxx"

#include <string>

struct Config {
	SocketConfig listener{
		.listen = 1024,
		.tcp_defer_accept = 10,
		.tcp_user_timeout = 20000,
		.tcp_no_delay = true,
	};

	SocketConfig knock_listener;

	std::string knock_nft_set;

	std::string user_database;

	AllocatedSocketAddress game_server;

	bool auto_reload_user_database = false;

	bool send_remote_ip = false;
};

Config
LoadConfigFile(const char *path);
