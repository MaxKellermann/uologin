// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "AccountedClientConnection.hxx"
#include "ClientAccounting.hxx"

AccountedClientConnection::~AccountedClientConnection() noexcept
{
	if (per_client != nullptr)
		per_client->RemoveConnection(*this);
}

void
AccountedClientConnection::UpdateTokenBucket(double size) noexcept
{
	if (per_client != nullptr)
		per_client->UpdateTokenBucket(size);
}
