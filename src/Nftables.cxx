// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Nftables.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "system/Error.hxx"
#include "util/ScopeExit.hxx"

#include <spawn.h>
#include <sys/wait.h>

void
NftAddElement(const char *family, const char *table, const char *set,
	      const char *address)
{
	const char*const argv[] = {
		"nft", "add", "element", family, table, set, "{", address, "}", nullptr,
	};

	posix_spawnattr_t attr;
	posix_spawnattr_init(&attr);
	AtScopeExit(&attr) { posix_spawnattr_destroy(&attr); };

	sigset_t signals;
	sigemptyset(&signals);
	posix_spawnattr_setsigmask(&attr, &signals);
	sigaddset(&signals, SIGCHLD);
	sigaddset(&signals, SIGHUP);
	sigaddset(&signals, SIGPIPE);
	posix_spawnattr_setsigdefault(&attr, &signals);

	posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGDEF|POSIX_SPAWN_SETSIGMASK);

	pid_t pid;
	if (posix_spawn(&pid, "/usr/sbin/nft", nullptr, &attr, const_cast<char *const*>(argv), nullptr) != 0)
		throw MakeErrno("Failed to execute `nft`");

	int status;

	do {
		if (waitpid(pid, &status, 0) < 0)
			throw MakeErrno("waitpid() failed");

		if (WIFSIGNALED(status))
			throw FmtRuntimeError("`nft` died from signal {}",
					      WTERMSIG(status));
	} while (!WIFEXITED(status));

	if (WEXITSTATUS(status) != EXIT_SUCCESS)
		throw FmtRuntimeError("`nft` exited with status {}", WEXITSTATUS(status));
}
