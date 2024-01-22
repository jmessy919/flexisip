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

#include "static-account-pool.hh"

#include "flexisip/logmanager.hh"

namespace flexisip::b2bua::bridge {
using namespace std;

StaticAccountPool::StaticAccountPool(linphone::Core& core,
                                     const std::shared_ptr<linphone::AccountParams>& params,
                                     const config::v2::AccountPoolName& poolName,
                                     const config::v2::AccountPool& pool,
                                     const config::v2::StaticLoader& loader) {
	const auto factory = linphone::Factory::get();
	reserve(loader.size());
	for (const auto& accountDesc : loader) {
		if (accountDesc.uri.empty()) {
			LOGF("An account of account pool '%s' is missing a `uri` field", poolName.c_str());
		}
		const auto address = core.createAddress(accountDesc.uri);
		params->setIdentityAddress(address);
		auto account = core.createAccount(params);
		core.addAccount(account);

		if (!accountDesc.password.empty()) {
			core.addAuthInfo(factory->createAuthInfo(address->getUsername(), accountDesc.userid, accountDesc.password,
			                                         "", "", address->getDomain()));
		}

		try_emplace(accountDesc.uri, accountDesc.alias,
		            make_shared<Account>(account, pool.maxCallsPerLine, accountDesc.alias));
	}
}
} // namespace flexisip::b2bua::bridge