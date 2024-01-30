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

AccountPool::AccountPool(linphone::Core& core,
                         const linphone::AccountParams& templateParams,
                         const config::v2::AccountPoolName& poolName,
                         const config::v2::AccountPool& pool,
                         unique_ptr<Loader>&& loader)
    : mLoader{std::move(loader)} {
	const auto factory = linphone::Factory::get();
	const auto accountsDesc = mLoader->initialLoad();

	reserve(accountsDesc.size());
	for (const auto& accountDesc : accountsDesc) {
		if (accountDesc.uri.empty()) {
			LOGF("An account of account pool '%s' is missing a `uri` field", poolName.c_str());
		}
		const auto address = core.createAddress(accountDesc.uri);
		const auto accountParams = templateParams.clone();
		accountParams->setIdentityAddress(address);

		if (!accountDesc.outboundProxy.empty()) {
			// Override global pool config if outboundProxy is present
			const auto route = core.createAddress(pool.outboundProxy);
			accountParams->setServerAddress(route);
			accountParams->setRoutesAddresses({route});
		}

		auto account = core.createAccount(accountParams);
		core.addAccount(account);

		if (!accountDesc.password.empty()) {
			core.addAuthInfo(factory->createAuthInfo(address->getUsername(), accountDesc.userid, accountDesc.password,
			                                         "", "", address->getDomain()));
		}

		try_emplace(accountDesc.uri, accountDesc.alias,
		            make_shared<Account>(account, pool.maxCallsPerLine, accountDesc.alias));
	}
}

std::shared_ptr<Account> AccountPool::getAccountByUri(const std::string& uri) const {
	if (const auto it = mAccountsByUri.find(uri); it != mAccountsByUri.cend()) {
		return it->second;
	}
	return nullptr;
}

std::shared_ptr<Account> AccountPool::getAccountByAlias(const string& alias) const {
	if (const auto it = mAccountsByAlias.find(alias); it != mAccountsByAlias.cend()) {
		return it->second;
	}
	return nullptr;
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
	auto [__, isInsertedAlias] = mAccountsByAlias.try_emplace(alias, account);
	if (!isInsertedAlias) {
		SLOGE << "AccountPool::try_emplace uri[" << alias << "] already present, account only inserted by uri.";
	}
}

} // namespace flexisip::b2bua::bridge