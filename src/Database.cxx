// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
