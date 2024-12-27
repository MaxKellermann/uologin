// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Database.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/CharUtil.hxx"
#include "util/SpanCast.hxx"

#include <sodium/crypto_pwhash.h>

#include <algorithm> // for std::transform()
#include <array>

Database::Database(const char *path)
{
	if (path != nullptr)
		db = BerkeleyDB{path};
}

bool
Database::CheckCredentials(std::string_view username,
			   std::string_view password) const
{
	if (!db)
		return true;

	std::array<char, 30> upper_username_buffer;
	if (username.size() > upper_username_buffer.size())
		return false;

	std::transform(username.begin(), username.end(),
		       upper_username_buffer.begin(), ToUpperASCII);
	username = {upper_username_buffer.data(), username.size()};

	std::array<std::byte, 256> value_buffer;

	auto value = FromBytesStrict<char>(db.Get(AsBytes(username), {value_buffer.data(), value_buffer.size() - 1}));
	if (value.data() == nullptr)
		return false;

	value[value.size()] = '\0';

	return crypto_pwhash_str_verify(value.data(),
					password.data(), password.size()) == 0;
}
