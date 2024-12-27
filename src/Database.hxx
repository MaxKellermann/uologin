// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "BerkeleyDB.hxx"

#include <string_view>

class Database {
	BerkeleyDB db;

public:
	[[nodiscard]]
	explicit Database(const char *path);

	~Database() noexcept = default;

	[[gnu::pure]]
	bool CheckCredentials(std::string_view username,
			      std::string_view password) const;
};
