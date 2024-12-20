// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#include "CommandLine.hxx"
#include "Instance.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/SystemError.hxx"
#include "util/PrintException.hxx"
#include "config.h"

#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include <pthread.h>
#include <stdlib.h>
#include <sys/signal.h>

static int
Run(const CommandLine &cmdline)
{
	Instance instance{cmdline.server};

	instance.AddListener(cmdline.listener.Create(SOCK_STREAM));

#ifdef HAVE_LIBSYSTEMD
	/* tell systemd we're ready */
	sd_notify(0, "READY=1");
#endif

	instance.GetEventLoop().Run();
	return EXIT_SUCCESS;
}

int
main(int argc, char **argv) noexcept
try {
	const auto cmdline = ParseCommandLine(argc, argv);

	signal(SIGPIPE, SIG_IGN);

	/* reduce glibc's thread cancellation overhead */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);

	return Run(cmdline);
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
