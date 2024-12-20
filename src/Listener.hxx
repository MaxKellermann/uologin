// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "event/net/ServerSocket.hxx"
#include "util/IntrusiveList.hxx"

class Instance;
class Connection;

class Listener final : ServerSocket {
	Instance &instance;

	IntrusiveList<Connection> connections;

public:
	[[nodiscard]]
	Listener(Instance &_instance, UniqueSocketDescriptor &&socket);
	~Listener() noexcept;

private:
	/* virtual methods from class ServerSocket */
	void OnAccept(UniqueSocketDescriptor fd,
		      SocketAddress address) noexcept override;
	void OnAcceptError(std::exception_ptr ep) noexcept override;
};
