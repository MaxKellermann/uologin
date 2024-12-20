// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Config.hxx"
#include "io/config/ConfigParser.hxx"
#include "io/config/FileLineParser.hxx"
#include "net/AddressInfo.hxx"
#include "net/IPv4Address.hxx"
#include "net/Resolver.hxx"
#include "util/StringAPI.hxx"

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
	} else if (StringIsEqual(word, "game_server")) {
		const char *value = line.ExpectValueAndEnd();

		static constexpr struct addrinfo hints = {
			.ai_flags = AI_ADDRCONFIG,
			.ai_family = AF_UNSPEC,
			.ai_socktype = SOCK_STREAM,
		};

		config.game_server = Resolve(value, 2593, &hints).GetBest();
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
	ParseConfigFile(path, parser);
	return config;
}

