// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#include "CommandLine.hxx"
#include "Config.hxx"
#include "Instance.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/SystemError.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/PrintException.hxx"
#include "config.h"

#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include <pthread.h>
#include <stdlib.h>
#include <sys/signal.h>

static int
Run(const Config &config)
{
	Instance instance{config};

	instance.AddListener(config.listener.Create(SOCK_STREAM));

	if (!config.knock_listener.bind_address.IsNull())
		instance.AddKnockListener(config.knock_listener.Create(SOCK_DGRAM),
					  config.knock_nft_set.empty() ? nullptr : config.knock_nft_set.c_str());

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
	const auto config = LoadConfigFile(cmdline.config_path);

	signal(SIGPIPE, SIG_IGN);

	/* reduce glibc's thread cancellation overhead */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);

	return Run(config);
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
