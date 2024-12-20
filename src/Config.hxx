// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/SocketConfig.hxx"

struct Config {
	SocketConfig listener{
		.listen = 1024,
		.tcp_defer_accept = 10,
		.tcp_user_timeout = 20000,
		.tcp_no_delay = true,
	};

	AllocatedSocketAddress game_server;

	Config() noexcept {
	}
};

Config
LoadConfigFile(const char *path);
