// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstddef>
#include <span>
#include <utility>

#include "db.h"

class BerkeleyDB {
	DB *db = nullptr;

public:
	[[nodiscard]]
	BerkeleyDB() noexcept = default;

	[[nodiscard]]
	explicit BerkeleyDB(const char *path) {
		Create();
		Open(path);
	}

	~BerkeleyDB() noexcept {
		if (db != nullptr)
			db->close(db, 0);
	}

	BerkeleyDB(BerkeleyDB &&src) noexcept
		:db(std::exchange(src.db, nullptr)) {}

	BerkeleyDB &operator=(BerkeleyDB &&src) noexcept {
		using std::swap;
		swap(db, src.db);
		return *this;
	}

	operator bool() const noexcept {
		return db != nullptr;
	}

	void Create();
	void Open(const char *path);

	/**
	 * @return the number of bytes written to #value_buffer; 0 for
	 * "key not found"
	 */
	[[nodiscard]]
	std::size_t Get(std::span<const std::byte> key,
			std::span<std::byte> value_buffer) const;
};
