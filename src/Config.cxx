// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Config.hxx"
#include "io/config/ConfigParser.hxx"
#include "io/config/FileLineParser.hxx"
#include "net/AddressInfo.hxx"
#include "net/IPv4Address.hxx"
#include "net/Parser.hxx"
#include "net/Resolver.hxx"
#include "util/StringAPI.hxx"

#include <fmt/core.h>

#include <stdlib.h> // for getenv()

using std::string_view_literals::operator""sv;

inline
Config::Config() noexcept
{
	if (const char *runtime_directory = getenv("RUNTIME_DIRECTORY")) {
		prometheus_exporter.bind_address.SetLocal(fmt::format("{}/prometheus-exporter.socket"sv,
								      runtime_directory));
	}
}

class MyConfigParser final : public ConfigParser {
	Config &config;

public:
	explicit MyConfigParser(Config &_config) noexcept
		:config(_config) {}

	// virtual methods from ConfigParser
	void ParseLine(FileLineParser &line) override;
	void Finish() override;
};

void
MyConfigParser::ParseLine(FileLineParser &line)
{
	const char *word = line.ExpectWord();

	if (StringIsEqual(word, "port")) {
		const uint16_t port = line.NextPositiveInteger();
		line.ExpectEnd();

		config.listener.bind_address = IPv4Address{port};
	} else if (StringIsEqual(word, "knock_port")) {
		const uint16_t port = line.NextPositiveInteger();
		line.ExpectEnd();

		config.knock_listener.bind_address = IPv4Address{port};
	} else if (StringIsEqual(word, "knock_nft_set")) {
		config.knock_nft_set = line.ExpectValueAndEnd();
	} else if (StringIsEqual(word, "user_database")) {
		config.user_database = line.ExpectValueAndEnd();
	} else if (StringIsEqual(word, "auto_reload_user_database")) {
		config.auto_reload_user_database = line.NextBool();
		line.ExpectEnd();
	} else if (StringIsEqual(word, "game_server")) {
		const char *value = line.ExpectValue();

		static constexpr struct addrinfo hints = {
			.ai_flags = AI_ADDRCONFIG,
			.ai_family = AF_UNSPEC,
			.ai_socktype = SOCK_STREAM,
		};

		config.game_server = Resolve(value, 2593, &hints).GetBest();

		if (!line.IsEnd()) {
			value = line.ExpectValueAndEnd();

			config.server_list.emplace_back(value, std::move(config.game_server));
		}
	} else if (StringIsEqual(word, "send_remote_ip")) {
		config.send_remote_ip = line.NextBool();
		line.ExpectEnd();
	} else if (StringIsEqual(word, "prometheus_exporter")) {
		const char *value = line.ExpectValueAndEnd();

		static constexpr struct addrinfo hints = {
			.ai_flags = AI_ADDRCONFIG|AI_PASSIVE,
			.ai_family = AF_UNSPEC,
			.ai_socktype = SOCK_STREAM,
		};

		config.prometheus_exporter.bind_address = ParseSocketAddress(value, 5476, hints);
	} else
		throw LineParser::Error{"Unknown option"};
}

void
MyConfigParser::Finish()
{
	if (config.listener.bind_address.IsNull())
		config.listener.bind_address = IPv4Address{2593};

	config.listener.Fixup();
	config.knock_listener.Fixup();

	if (config.game_server.IsNull())
		throw "No game_server setting";
}

Config
LoadConfigFile(const char *path)
{
	Config config;
	MyConfigParser parser{config};
	CommentConfigParser parser2{parser};
	ParseConfigFile(path, parser2);
	return config;
}

