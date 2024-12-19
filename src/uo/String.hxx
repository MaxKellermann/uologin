// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <algorithm> // for std::find()
#include <span>
#include <string_view>

namespace UO {

constexpr std::string_view
ExtractString(std::span<const char> src) noexcept
{
	const auto i = std::find(src.begin(), src.end(), '\0');
	return {src.begin(), i};
}

} // namespace UO
