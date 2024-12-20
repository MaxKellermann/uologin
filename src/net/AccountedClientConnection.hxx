// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "util/IntrusiveList.hxx"

class PerClientAccounting;

class AccountedClientConnection {
	friend class PerClientAccounting;

	IntrusiveListHook<IntrusiveHookMode::NORMAL> siblings;

	PerClientAccounting *per_client = nullptr;

public:
	using List = IntrusiveList<
		AccountedClientConnection,
		IntrusiveListMemberHookTraits<&AccountedClientConnection::siblings>,
		IntrusiveListOptions{.constant_time_size = true}>;

	AccountedClientConnection() = default;
	~AccountedClientConnection() noexcept;

	AccountedClientConnection(const AccountedClientConnection &) = delete;
	AccountedClientConnection &operator=(const AccountedClientConnection &) = delete;

	PerClientAccounting *GetPerClient() const noexcept {
		return per_client;
	}

	void UpdateTokenBucket(double size) noexcept;
};
