// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <string_view>

class Database {
public:
	[[gnu::pure]]
	bool CheckCredentials(std::string_view username,
			      std::string_view password) const noexcept;
};
