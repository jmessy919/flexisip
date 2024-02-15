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

#include <memory>
#include <unordered_map>

#include "flexisip/sofia-wrapper/su-root.hh"

#include "b2bua/sip-bridge/accounts/account.hh"
#include "b2bua/sip-bridge/accounts/loaders/loader.hh"
#include "b2bua/sip-bridge/configuration/v2/v2.hh"
#include "libhiredis-wrapper/redis-async-session.hh"

#pragma once

namespace flexisip::b2bua::bridge {

class AccountPool {
public:
	AccountPool(const std::shared_ptr<sofiasip::SuRoot>& suRoot,
	            linphone::Core& core,
	            const linphone::AccountParams& templateParams,
	            const config::v2::AccountPoolName& poolName,
	            const config::v2::AccountPool& pool,
	            std::unique_ptr<Loader>&& loader);

	// Disable copy semantics
	AccountPool(const AccountPool&) = delete;
	AccountPool& operator=(const AccountPool&) = delete;

	std::shared_ptr<Account> getAccountByUri(const std::string& uri) const;
	std::shared_ptr<Account> getAccountByAlias(const std::string& alias) const;
	std::shared_ptr<Account> getAccountRandomly() const;

	auto size() const {
		return mAccountsByUri.size();
	}
	auto begin() const {
		return mAccountsByUri.begin();
	}
	auto end() const {
		return mAccountsByUri.end();
	}

private:
	void reserve(size_t sizeToReserve);
	void try_emplace(const std::string& uri, const std::string& alias, const std::shared_ptr<Account>& account);

	void accountUpdateNeeded(const std::string& username, const std::string& domain, const std::string& identifier);
	void onAccountUpdate(config::v2::Account accountToUpdate);

	std::shared_ptr<sofiasip::SuRoot> mSuRoot;
	std::unique_ptr<Loader> mLoader;
	std::unordered_map<std::string, std::shared_ptr<Account>> mAccountsByUri;
	std::unordered_map<std::string, std::shared_ptr<Account>> mAccountsByAlias;
	std::unique_ptr<redis::async::SubscriptionSession> session{nullptr};
};

} // namespace flexisip::b2bua::bridge