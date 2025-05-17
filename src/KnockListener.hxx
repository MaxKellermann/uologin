// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "event/net/MultiUdpListener.hxx"
#include "event/net/UdpHandler.hxx"

struct Config;
class Instance;
class UniqueSocketDescriptor;

class KnockListener final : UdpHandler {
	Instance &instance;
	MultiUdpListener udp_listener;
	const char *const nft_set;

	class Request;

public:
	[[nodiscard]]
	KnockListener(Instance &_instance, UniqueSocketDescriptor &&socket,
		      const char *_nft_set);

private:
	// virtual methods from UdpHandler
	bool OnUdpDatagram(std::span<const std::byte> payload,
			   std::span<UniqueFileDescriptor> fds,
			   SocketAddress address, int uid) override;
	void OnUdpError(std::exception_ptr error) noexcept override;
};
