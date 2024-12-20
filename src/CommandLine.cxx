// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CommandLine.hxx"

CommandLine
ParseCommandLine(int argc, char **argv)
{
	if (argc > 2)
		throw "Usage: uosagas-login-server [CONFIGFILE]";

	CommandLine cmdline;

	if (argc >= 2)
		cmdline.config_path = argv[1];

	return cmdline;
}

