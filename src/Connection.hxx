// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Splice.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "event/SocketEvent.hxx"
#include "event/net/ConnectSocket.hxx"
#include "net/AccountedClientConnection.hxx"
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

	AccountedClientConnection accounting;

	SocketEvent incoming, outgoing;

	Splice splice_in_out, splice_out_in;

	ConnectSocket connect;

	CoarseTimerEvent timeout;

	std::array<std::byte, 83> initial_packets;
	uint_least8_t initial_packets_fill = 0;

	enum class State : uint_least8_t {
		INITIAL,
		CONNECTING,
		READY,
	} state = State::INITIAL;

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

	void OnIncomingReady(unsigned events) noexcept;
	void OnOutgoingReady(unsigned events) noexcept;
	void OnTimeout() noexcept;

	/* virtual methods from ConnectSocketHandler */
	void OnSocketConnectSuccess(UniqueSocketDescriptor fd) noexcept override;
	void OnSocketConnectError(std::exception_ptr e) noexcept override;
};
