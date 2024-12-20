// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Database.hxx"

bool
Database::CheckCredentials(std::string_view username,
			   std::string_view password) const noexcept
{
	// TODO implement
	(void)username;
	(void)password;
	return true;
}
