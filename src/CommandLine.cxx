// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CommandLine.hxx"
#include "net/AddressInfo.hxx"
#include "net/IPv4Address.hxx"
#include "net/Resolver.hxx"

#include <fmt/core.h>

CommandLine
ParseCommandLine(int argc, char **argv)
{
	if (argc != 3)
		throw "Usage: uosagas-login-server PORT SERVER";

	const uint16_t port = atoi(argv[1]);

	CommandLine cmdline;

	cmdline.listener.bind_address = IPv4Address{port};
	cmdline.listener.listen = 1024;
	cmdline.listener.tcp_defer_accept = 10;
	cmdline.listener.tcp_user_timeout = 20000;
	cmdline.listener.tcp_no_delay = true;

	static constexpr struct addrinfo hints = {
		.ai_flags = AI_ADDRCONFIG,
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};

	cmdline.server = Resolve(argv[2], 2593, &hints).GetBest();

	return cmdline;
}

