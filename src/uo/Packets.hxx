// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "util/PackedBigEndian.hxx"

#include <cstdint>

namespace UO {

enum class Command : uint8_t;

struct CredentialsFragment {
	char username[30];
	char password[30];
};

static_assert(alignof(CredentialsFragment) == 1);
static_assert(sizeof(CredentialsFragment) == 60);

} // namespace UO

/* 0x80 AccountLogin */
struct uo_packet_account_login {
	UO::Command cmd;
	UO::CredentialsFragment credentials;
	std::byte unknown1;
};

static_assert(alignof(struct uo_packet_account_login) == 1);
static_assert(sizeof(struct uo_packet_account_login) == 62);

/* 0x82 AccountLoginReject */
struct uo_packet_account_login_reject {
	UO::Command cmd;
	uint8_t reason;
};

static_assert(alignof(struct uo_packet_account_login_reject) == 1);

/* 0x91 GameLogin */
struct uo_packet_game_login {
	UO::Command cmd;
	PackedBE32 auth_id;
	UO::CredentialsFragment credentials;
};

static_assert(alignof(struct uo_packet_game_login) == 1);
static_assert(sizeof(struct uo_packet_game_login) == 65);

/* for 0xa0 PlayServer */
struct uo_packet_play_server {
	UO::Command cmd;
	PackedBE16 index;
};

static_assert(alignof(struct uo_packet_play_server) == 1);

/* for 0xa8 ServerList */
struct uo_fragment_server_info {
	PackedBE16 index;
	char name[32];
	char full;
	uint8_t timezone;
	PackedBE32 address;
};

static_assert(alignof(struct uo_fragment_server_info) == 1);

/* 0xa8 ServerList */
struct uo_packet_server_list {
	UO::Command cmd;
	PackedBE16 length;
	uint8_t unknown_0x5d;
	PackedBE16 num_game_servers;
	struct uo_fragment_server_info game_servers[1];
};

static_assert(alignof(struct uo_packet_server_list) == 1);

/* 0xbf Extended */
struct uo_packet_extended {
	UO::Command cmd;
	PackedBE16 length;
	PackedBE16 extended_cmd;
};

static_assert(alignof(struct uo_packet_extended) == 1);

/* 0xef Seed */
struct uo_packet_seed {
	UO::Command cmd;
	PackedBE32 seed;
	PackedBE32 client_major;
	PackedBE32 client_minor;
	PackedBE32 client_revision;
	PackedBE32 client_patch;
};

static_assert(alignof(struct uo_packet_seed) == 1);
