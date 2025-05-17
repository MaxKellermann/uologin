// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Database.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/SystemError.hxx"
#include "thread/Job.hxx"
#include "thread/Queue.hxx"
#include "thread/Pool.hxx"
#include "io/CopyRegularFile.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/Cancellable.hxx"
#include "util/CharUtil.hxx"
#include "util/SpanCast.hxx"

#include <sodium/crypto_pwhash.h>

#include <algorithm> // for std::transform()
#include <array>
#include <cassert>

#include <fcntl.h> // for O_CREAT
#include <stdlib.h> // for getenv()
#include <sys/stat.h>

using std::string_view_literals::operator""sv;

Database::Database(ThreadQueue &_queue, const char *_path, bool _auto_reload)
	:queue(_queue), path(_path), auto_reload(_auto_reload)
{
	if (path != nullptr && !auto_reload)
		db = BerkeleyDB{path};
}

static void
CopyRegularFile(const char *src, const char *dst, off_t size)
{
	const auto src_fd = OpenReadOnly(src);
	const auto dst_fd = OpenWriteOnly(dst, O_CREAT|O_TRUNC);
	CopyRegularFile(src_fd, dst_fd, size);
}

inline void
Database::DoAutoReload(off_t size)
{
	assert(auto_reload);

	const char *runtime_directory = getenv("RUNTIME_DIRECTORY");
	if (runtime_directory == nullptr)
		throw std::runtime_error{"No RUNTIME_DIRECTORY"};

	const auto copy_path = fmt::format("{}/user.db"sv, runtime_directory);
	CopyRegularFile(path, copy_path.c_str(), size);

	db = BerkeleyDB{copy_path.c_str()};
}

inline void
Database::MaybeAutoReload()
{
	assert(auto_reload);

	struct stat st;
	if (stat(path, &st) < 0)
		throw FmtErrno("Failed to check {:?}", path);

	if (!S_ISREG(st.st_mode))
		throw FmtRuntimeError("Not a regular file: {:?}", path);

	if (st.st_mtime == last_mtime) {
		/* not modified, but the last attempt may have failed,
		   so rethrow any previous error */
		if (last_reload_error)
			std::rethrow_exception(last_reload_error);
		return;
	}

	last_mtime = st.st_mtime;
	db = {};
	last_reload_error = {};

	try {
		DoAutoReload(st.st_size);
	} catch (...) {
		last_reload_error = std::current_exception();
		throw;
	}
}

class Database::CheckCredentialsJob final : public ThreadJob, Cancellable {
	ThreadQueue &queue;

	const std::string username;

	std::array<std::byte, 256> value_buffer;

	const std::span<char> value;

	const std::string_view password;

	const CheckCredentialsCallback callback;

	bool canceled = false;

	bool result;

public:
	explicit CheckCredentialsJob(ThreadQueue &_queue, BerkeleyDB &db, std::string_view _username,
				     std::string_view _password,
				     CheckCredentialsCallback _callback,
				     CancellablePointer &cancel_ptr)
		:queue(_queue), username(_username),
		 value(FromBytesStrict<char>(db.Get(AsBytes(username), {value_buffer.data(), value_buffer.size() - 1}))),
		 password(_password), callback(_callback) {
		value[value.size()] = '\0';
		cancel_ptr = *this;
	}

	void Run() noexcept override {
		result = crypto_pwhash_str_verify(value.data(), password.data(), password.size()) == 0;
	}

	void Done() noexcept override {
		if (!canceled)
			callback(username, result);
		delete this;
	}

	void Cancel() noexcept override {
		canceled = true;

		if (queue.Cancel(*this))
			delete this;
	}
};

void
Database::CheckCredentials(std::string_view username,
			   std::string_view password,
			   CheckCredentialsCallback callback,
			   CancellablePointer &cancel_ptr)
{
	if (auto_reload)
		MaybeAutoReload();

	if (!db) {
		callback(username, true);
		return;
	}

	std::array<char, 30> upper_username_buffer;
	if (username.size() > upper_username_buffer.size()) {
		callback(username, false);
		return;
	}

	std::transform(username.begin(), username.end(),
		       upper_username_buffer.begin(), ToUpperASCII);
	username = {upper_username_buffer.data(), username.size()};

	auto *job = new CheckCredentialsJob(queue, db, username, password, callback, cancel_ptr);
	queue.Add(*job);
}
