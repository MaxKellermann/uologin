// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#include "BerkeleyDB.hxx"
#include "lib/fmt/RuntimeError.hxx"

#include <cassert>

void
BerkeleyDB::Create()
{
	assert(db == nullptr);

	if (int ret = db_create(&db, nullptr, 0))
		throw FmtRuntimeError("db_create() failed: {}", db_strerror(ret));
}

void
BerkeleyDB::Open(const char *path)
{
	assert(db != nullptr);

	constexpr u_int32_t flags = DB_RDONLY;

	if (int ret = db->open(db, nullptr, path, nullptr, DB_HASH, flags, 0)) {
		db->close(db, 0);
		throw FmtRuntimeError("db_open() failed: {}", db_strerror(ret));
	}
}

std::size_t
BerkeleyDB::Get(std::span<const std::byte> key,
		std::span<std::byte> value_buffer) const
{
	assert(db != nullptr);

	DBT db_key{
		.data = const_cast<std::byte *>(key.data()),
		.size = static_cast<u_int32_t>(key.size()),
	};

	DBT db_value{
		.data = value_buffer.data(),
		.ulen = static_cast<u_int32_t>(value_buffer.size()),
		.flags = DB_DBT_USERMEM,
	};

	if (int ret = db->get(db, nullptr, &db_key, &db_value, 0)) {
		if (ret == DB_NOTFOUND)
			return {};

		throw FmtRuntimeError("db_get() failed: {}", db_strerror(ret));
	}

	return db_value.size;
}
