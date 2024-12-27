// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#include "KnockListener.hxx"
#include "Instance.hxx"
#include "Validate.hxx"
#include "Nftables.hxx"
#include "uo/Command.hxx"
#include "uo/Packets.hxx"
#include "uo/String.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "net/MultiReceiveMessage.hxx"
#include "net/ToString.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/PrintException.hxx"

KnockListener::KnockListener(Instance &_instance, UniqueSocketDescriptor &&socket,
			     const char *_nft_set)
	:instance(_instance),
	 udp_listener(instance.GetEventLoop(), std::move(socket),
		      MultiReceiveMessage{1024, sizeof(struct uo_packet_account_login)},
		      *this),
	 nft_set(_nft_set)
{
}

bool
KnockListener::OnUdpDatagram(std::span<const std::byte> payload,
			     std::span<UniqueFileDescriptor>,
			     SocketAddress address, int)
{
	auto *accounting = instance.GetClientAccounting(address);
	if (accounting == nullptr)
		return true;

	const auto &packet = *reinterpret_cast<const struct uo_packet_account_login *>(payload.data());
	if (payload.size() != sizeof(packet) ||
	    packet.cmd != UO::Command::AccountLogin) {
		if (accounting != nullptr)
			accounting->UpdateTokenBucket(10);
		return true;
	}

	const auto username = UO::ExtractString(packet.credentials.username);
	const auto password = UO::ExtractString(packet.credentials.password);
	if (!IsValidUsername(username)) {
		if (accounting != nullptr)
			accounting->UpdateTokenBucket(8);
		return true;
	}

	if (!instance.GetDatabase().CheckCredentials(username, password)) {
		if (accounting != nullptr)
			accounting->UpdateTokenBucket(5);
		return true;
	}

	accounting->SetKnocked();

	if (nft_set != nullptr) {
		if (const auto ip = HostToString(address); !ip.empty()) {
			try {
				NftAddElement("inet", "filter", nft_set, ip.c_str());
			} catch (...) {
				fmt::print(stderr, "Failed to add nft element: {}\n", std::current_exception());
			}
		}
	}

	return true;
}

void
KnockListener::OnUdpError(std::exception_ptr error) noexcept
{
	PrintException(std::move(error));
}
