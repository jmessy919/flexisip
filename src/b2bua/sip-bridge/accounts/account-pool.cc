/*
    Flexisip, a flexible SIP proxy server with media capabilities.
    Copyright (C) 2010-2024 Belledonne Communications SARL, All rights reserved.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "account-pool.hh"

#include "flexisip/logmanager.hh"

namespace flexisip::b2bua::bridge {
using namespace std;

std::shared_ptr<Account> AccountPool::getAccountByUri(const std::string& uri) const {
	try {
		return mAccountsByUri.at(uri);
	} catch (out_of_range&) {
		return nullptr;
	}
}

std::shared_ptr<Account> AccountPool::getAccountByAlias(const string& alias) const {
	try {
		return mAccountsByAlias.at(alias);
	} catch (out_of_range&) {
		return nullptr;
	}
}

std::shared_ptr<Account> AccountPool::getAccountRandomly() const {
	// Pick a random account then keep iterating if unavailable
	const auto max = size();
	const auto seed = rand() % max;
	auto poolIt = begin();

	for (auto i = 0UL; i < seed; i++) {
		poolIt++;
	}

	for (auto i = 0UL; i < max; i++) {
		if (const auto& account = poolIt->second; account->isAvailable()) {
			return account;
		}

		poolIt++;
		if (poolIt == end()) poolIt = begin();
	}

	return nullptr;
}

void AccountPool::reserve(size_t sizeToReserve) {
	mAccountsByUri.reserve(sizeToReserve);
	mAccountsByAlias.reserve(sizeToReserve);
}

void AccountPool::try_emplace(const string& uri, const string& alias, const shared_ptr<Account>& account) {
	if (uri.empty()) {
		SLOGE << "AccountPool::try_emplace called with empty uri, nothing happened";
		return;
	}

	auto [_, isInsertedUri] = mAccountsByUri.try_emplace(uri, account);
	if (!isInsertedUri) {
		SLOGE << "AccountPool::try_emplace uri[" << uri << "] already present, nothing happened";
		return;
	}

	if (alias.empty()) return;
	auto [__, isInsertedAlias] = mAccountsByUri.try_emplace(uri, account);
	if (!isInsertedAlias) {
		SLOGE << "AccountPool::try_emplace uri[" << alias << "] already present, account only inserted by uri.";
	}
}

} // namespace flexisip::b2bua::bridge