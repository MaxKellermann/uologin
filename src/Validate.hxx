// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "util/CharUtil.hxx"
#include "util/StringVerify.hxx"

constexpr bool
IsValidUsername(std::string_view s) noexcept
{
	return CheckCharsNonEmpty(s, [](char ch){
		return IsPrintableASCII(ch);
	});
}
