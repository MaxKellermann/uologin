// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "BerkeleyDB.hxx"
#include "util/BindMethod.hxx"

#include <exception>
#include <string_view>

#include <sys/types.h>
#include <time.h>

class EventLoop;
class CancellablePointer;

class Database {
	EventLoop &event_loop;

	BerkeleyDB db;

	const char *const path;

	time_t last_mtime{};

	std::exception_ptr last_reload_error;

	const bool auto_reload;

	class CheckCredentialsJob;

public:
	[[nodiscard]]
	Database(EventLoop &_event_loop, const char *_path, bool _auto_reload);

	~Database() noexcept = default;

	using CheckCredentialsCallback = BoundMethod<void(std::string_view username, bool result) noexcept>;

	void CheckCredentials(std::string_view username,
			      std::string_view password,
			      CheckCredentialsCallback callback,
			      CancellablePointer &cancel_ptr);

private:
	void MaybeAutoReload();
	void DoAutoReload(off_t size);
};
