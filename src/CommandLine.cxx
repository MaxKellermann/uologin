// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

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

