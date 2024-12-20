// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "AccountedClientConnection.hxx"
#include "event/FarTimerEvent.hxx"
#include "util/IntrusiveHashSet.hxx"
#include "util/TokenBucket.hxx"

#include <cstdint>

class SocketAddress;
class PerClientAccounting;
class ClientAccountingMap;

class PerClientAccounting final
	: public IntrusiveHashSetHook<IntrusiveHookMode::AUTO_UNLINK>
{
	friend class ClientAccountingMap;

	ClientAccountingMap &map;

	const uint_least64_t address;

	struct GetKey {
		constexpr uint_least64_t operator()(const PerClientAccounting &item) const noexcept {
			return item.address;
		}
	};

	using ConnectionList = AccountedClientConnection::List;

	ConnectionList connections;

	Event::TimePoint expires;

	/**
	 * After this time point, the delay can be cleared.
	 */
	Event::TimePoint tarpit_until;

	/**
	 * The current delay (for the server greeting and
	 * authentication).
	 */
	Event::Duration delay{};

	TokenBucket token_bucket;

public:
	PerClientAccounting(ClientAccountingMap &_map, uint_least64_t _address) noexcept;

	[[gnu::pure]]
	bool Check() const noexcept;

	void AddConnection(AccountedClientConnection &c) noexcept;
	void RemoveConnection(AccountedClientConnection &c) noexcept;

	void UpdateTokenBucket(double size) noexcept;

	Event::Duration GetDelay() const noexcept {
		return delay;
	}

private:
	[[gnu::pure]]
	Event::TimePoint Now() const noexcept;
};

class ClientAccountingMap {
	const std::size_t max_connections;

	const bool tarpit;

	using Map = IntrusiveHashSet<PerClientAccounting, 65536,
				     IntrusiveHashSetOperators<PerClientAccounting,
							       PerClientAccounting::GetKey,
							       std::hash<uint_least64_t>,
							       std::equal_to<uint_least64_t>>>;
	Map map;

	FarTimerEvent cleanup_timer;

public:
	ClientAccountingMap(EventLoop &event_loop, std::size_t _max_connections,
			    bool _tarpit) noexcept
		:max_connections(_max_connections),
		 tarpit(_tarpit),
		 cleanup_timer(event_loop, BIND_THIS_METHOD(OnCleanupTimer)) {}

	auto &GetEventLoop() const noexcept {
		return cleanup_timer.GetEventLoop();
	}

	std::size_t GetMaxConnections() const noexcept {
		return max_connections;
	}

	bool HasTarpit() const noexcept {
		return tarpit;
	}

	PerClientAccounting *Get(SocketAddress address) noexcept;

	void ScheduleCleanup() noexcept;

private:
	void OnCleanupTimer() noexcept;
};
