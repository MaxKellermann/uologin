// SPDX-License-Identifier: GPL-2.0-or-later
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Splice.hxx"
#include "PipeStock.hxx"
#include "stock/Item.hxx"
#include "net/SocketDescriptor.hxx"

#include <cassert>

#include <errno.h>
#include <fcntl.h> // for splice()

Splice::~Splice() noexcept
{
	if (pipe != nullptr)
		pipe->Put(size == 0 ? PutAction::REUSE : PutAction::DESTROY);
}

Splice::ReceiveResult
Splice::ReceiveFrom(PipeStock &stock, SocketDescriptor s)
{
	if (pipe == nullptr) {
		assert(size == 0);
		pipe = stock.GetNow(nullptr);
	}

	const auto nbytes = splice(s.Get(), nullptr,
				   pipe_stock_item_get(pipe).second.Get(), nullptr,
				   1ULL << 30,
				   SPLICE_F_MOVE|SPLICE_F_NONBLOCK);
	if (nbytes > 0) {
		size += static_cast<std::size_t>(nbytes);
		received_bytes += static_cast<std::size_t>(nbytes);
		return ReceiveResult::OK;
	} else if (nbytes == 0) {
		if (size == 0) {
			pipe->Put(PutAction::REUSE);
			pipe = nullptr;
		}

		return ReceiveResult::SOCKET_CLOSED;
	} else {
		const int e = errno;
		if (e == EAGAIN)
			return size == 0
				? ReceiveResult::SOCKET_BLOCKING
				: ReceiveResult::PIPE_FULL;

		return ReceiveResult::ERROR;
	}
}

Splice::SendResult
Splice::SendTo(SocketDescriptor s)
{
	assert(pipe != nullptr);
	assert(size > 0);

	const auto nbytes = splice(pipe_stock_item_get(pipe).first.Get(), nullptr,
				   s.Get(), nullptr,
				   size,
				   SPLICE_F_MOVE|SPLICE_F_NONBLOCK);
	if (nbytes > 0) {
		size -= static_cast<std::size_t>(nbytes);
		sent_bytes += static_cast<std::size_t>(nbytes);

		if (size == 0) {
			pipe->Put(PutAction::REUSE);
			pipe = nullptr;
			return SendResult::OK;
		} else
			return SendResult::PARTIAL;
	} else if (nbytes == 0) {
		// TODO should not happen
		errno = EINVAL;
		return SendResult::ERROR;
	} else {
		const int e = errno;
		if (e == EAGAIN)
			return SendResult::SOCKET_BLOCKING;

		return SendResult::ERROR;
	}
}
