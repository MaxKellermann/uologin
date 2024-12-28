// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "net/AllocatedSocketAddress.hxx"
#include "net/SocketConfig.hxx"

#include <string>
#include <vector>

struct GameServerConfig {
	const std::string name;
	AllocatedSocketAddress address;

	[[nodiscard]]
	GameServerConfig(const char *_name, SocketAddress _address) noexcept
		:name(_name), address(_address) {}
};

struct Config {
	SocketConfig prometheus_exporter{
		.listen = 16,
		.tcp_defer_accept = 10,
		.mode = 0666,
		.tcp_no_delay = true,
	};

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

	std::vector<GameServerConfig> server_list;

	bool auto_reload_user_database = false;

	bool send_remote_ip = false;

	Config() noexcept;
};

Config
LoadConfigFile(const char *path);
