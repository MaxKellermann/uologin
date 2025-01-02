// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "net/SocketConfig.hxx"

struct CommandLine {
	const char *config_path = "/etc/uologin.conf";
};

CommandLine
ParseCommandLine(int argc, char **argv);
