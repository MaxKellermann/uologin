// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Splice.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "event/SocketEvent.hxx"
#include "event/net/ConnectSocket.hxx"
#include "net/AccountedClientConnection.hxx"
#include "net/StaticSocketAddress.hxx"
#include "util/IntrusiveList.hxx"

#include <array>

class Instance;
class UniqueSocketDescriptor;
class SocketAddress;

class Connection final
	: public IntrusiveListHook<IntrusiveHookMode::AUTO_UNLINK>,
	  ConnectSocketHandler
{
	Instance &instance;

	const StaticSocketAddress remote_address;

	AccountedClientConnection accounting;

	SocketEvent incoming, outgoing;

	Splice splice_in_out, splice_out_in;

	SocketAddress outgoing_address;
	ConnectSocket connect;

	CoarseTimerEvent timeout;

	CancellablePointer cancel_ptr;

	std::array<std::byte, 83> initial_packets;
	uint_least8_t initial_packets_fill = 0;

	enum class State : uint_least8_t {
		INITIAL,
		CHECK_CREDENTIALS,
		SERVER_LIST,
		CONNECTING,
		SEND_PLAY_SERVER,
		READY,
	} state = State::INITIAL;

	bool send_play_server = false;

public:
	Connection(Instance &_instance,
		   PerClientAccounting *per_client,
		   UniqueSocketDescriptor &&_fd, SocketAddress address) noexcept;
	~Connection() noexcept;

	auto &GetEventLoop() const noexcept {
		return connect.GetEventLoop();
	}

private:
	void Destroy() noexcept {
		delete this;
	}

	bool SendAccountLoginReject() noexcept;

	void SendServerList() noexcept;
	void ReceivePlayServer() noexcept;
	void ReceiveLoginPackets() noexcept;

	void ReceiveServerList() noexcept;

	void OnIncomingReady(unsigned events) noexcept;
	void OnOutgoingReady(unsigned events) noexcept;
	void OnTimeout() noexcept;

	void OnCheckCredentials(std::string_view username, bool result) noexcept;

	bool SendInitialPackets(SocketDescriptor socket) noexcept;

	/* virtual methods from ConnectSocketHandler */
	void OnSocketConnectSuccess(UniqueSocketDescriptor fd) noexcept override;
	void OnSocketConnectError(std::exception_ptr e) noexcept override;
};
