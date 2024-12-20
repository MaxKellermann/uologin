// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Connection.hxx"
#include "event/net/TemplateServerSocket.hxx"

using Listener =
	TemplateServerSocket<Connection,
			     Instance &>;
