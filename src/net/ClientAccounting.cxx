// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ClientAccounting.hxx"
#include "event/Loop.hxx"
#include "net/SocketAddress.hxx"
#include "net/IPv4Address.hxx"
#include "net/IPv6Address.hxx"
#include "time/Cast.hxx"
#include "util/DeleteDisposer.hxx"

static constexpr uint_least64_t
Read64(const uint8_t *src) noexcept
{
	uint_least64_t value{};
	for (unsigned i = 0; i < 8; ++i)
		value = (value << 8) | src[i];
	return value;
}

static constexpr uint_least64_t
ToInteger(const struct in6_addr &addr) noexcept
{
	return Read64(addr.s6_addr) ^ Read64(addr.s6_addr + 8);
}

static uint_least64_t
ToInteger(SocketAddress address) noexcept
{
	if (address.IsNull())
		return 0;

	switch (address.GetFamily()) {
	case AF_INET:
		return IPv4Address::Cast(address).GetNumericAddressBE();

	case AF_INET6:
		if (const auto &v6 = IPv6Address::Cast(address); v6.IsV4Mapped())
			return v6.UnmapV4().GetNumericAddressBE();

		return ToInteger(IPv6Address::Cast(address).GetAddress());

	default:
		return 0;
	}
}

PerClientAccounting::PerClientAccounting(ClientAccountingMap &_map,
					 uint_least64_t _address) noexcept
	:map(_map), address(_address)
{
}

inline Event::TimePoint
PerClientAccounting::Now() const noexcept
{
	return map.GetEventLoop().SteadyNow();
}

bool
PerClientAccounting::Check() const noexcept
{
	const std::size_t max_connections = map.GetMaxConnections();
	return max_connections == 0 || connections.size() < max_connections;
}

void
PerClientAccounting::AddConnection(AccountedClientConnection &c) noexcept
{
	assert(c.per_client == nullptr);

	connections.push_back(c);
	c.per_client = this;
}

void
PerClientAccounting::RemoveConnection(AccountedClientConnection &c) noexcept
{
	assert(c.per_client == this);

	connections.erase(connections.iterator_to(c));
	c.per_client = nullptr;

	expires = Now() + std::chrono::minutes{5};

	if (connections.empty())
		map.ScheduleCleanup();
}

void
PerClientAccounting::UpdateTokenBucket(double size) noexcept
{
	if (!map.HasTarpit())
		return;

	constexpr double RATE = 1, BURST = 10;
	constexpr Event::Duration TARPIT_FOR = std::chrono::minutes{1};
	constexpr Event::Duration MAX_DELAY = std::chrono::minutes{1};
	constexpr Event::Duration DELAY_STEP = std::chrono::milliseconds{100};

	const auto now = map.GetEventLoop().SteadyNow();
	double available = token_bucket.Update(ToFloatSeconds(now.time_since_epoch()), RATE, BURST, size);
	if (available < 0) {
		tarpit_until = now + TARPIT_FOR;

		if (delay < MAX_DELAY)
			delay += DELAY_STEP;
	} else if (now < tarpit_until) {
		if (delay > DELAY_STEP)
			delay -= DELAY_STEP;
	} else
		delay = {};
}

PerClientAccounting *
ClientAccountingMap::Get(SocketAddress _address) noexcept
{
	const uint_least64_t address = ToInteger(_address);
	if (address == 0)
		return nullptr;

	auto [i, inserted] = map.insert_check(address);
	if (inserted) {
		auto *per_client = new PerClientAccounting(*this, address);
		map.insert_commit(i, *per_client);
		return per_client;
	} else
		return &*i;

}

void
ClientAccountingMap::ScheduleCleanup() noexcept
{
	if (!cleanup_timer.IsPending())
		cleanup_timer.Schedule(std::chrono::minutes{1});
}

void
ClientAccountingMap::OnCleanupTimer() noexcept
{
	bool reschedule = false;

	const auto now = GetEventLoop().SteadyNow();

	map.remove_and_dispose_if([&reschedule, now](const auto &i){
		if (!i.connections.empty())
			return false;

		if (now < i.expires) {
			reschedule = true;
			return false;
		}

		return true;
	}, DeleteDisposer{});

	if (reschedule)
		ScheduleCleanup();
}
