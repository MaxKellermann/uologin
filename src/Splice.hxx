// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstddef>

class PipeStock;
class StockItem;
class SocketDescriptor;

class Splice final {
	StockItem *pipe = nullptr;

	std::size_t size = 0;

public:
	Splice() noexcept = default;
	~Splice() noexcept;

	Splice(const Splice &) = delete;
	Splice &operator=(const Splice &) = delete;

	bool IsEmpty() const noexcept {
		return size == 0;
	}

	enum class ReceiveResult {
		OK,
		SOCKET_BLOCKING,
		SOCKET_CLOSED,
		PIPE_FULL,
		ERROR,
	};

	ReceiveResult ReceiveFrom(PipeStock &stock, SocketDescriptor s);

	enum class SendResult {
		OK,
		PARTIAL,
		SOCKET_BLOCKING,
		ERROR,
	};

	SendResult SendTo(SocketDescriptor s);
};
