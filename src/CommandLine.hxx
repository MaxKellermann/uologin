// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/SocketConfig.hxx"

struct CommandLine {
	const char *config_path = "/etc/uosagas/login.conf";
};

CommandLine
ParseCommandLine(int argc, char **argv);
