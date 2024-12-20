// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <string_view>

class Database {
public:
	[[gnu::pure]]
	bool CheckCredentials(std::string_view username,
			      std::string_view password) const noexcept;
};
