// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "BerkeleyDB.hxx"

#include <exception>
#include <string_view>

#include <sys/types.h>
#include <time.h>

class Database {
	BerkeleyDB db;

	const char *const path;

	time_t last_mtime{};

	std::exception_ptr last_reload_error;

	const bool auto_reload;

public:
	[[nodiscard]]
	Database(const char *_path, bool _auto_reload);

	~Database() noexcept = default;

	[[gnu::pure]]
	bool CheckCredentials(std::string_view username,
			      std::string_view password);

private:
	void MaybeAutoReload();
	void DoAutoReload(off_t size);
};
