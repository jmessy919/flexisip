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
using namespace flexisip::redis;
using namespace flexisip::redis::async;

AccountPool::AccountPool(const std::shared_ptr<sofiasip::SuRoot>& suRoot,
                         const std::shared_ptr<linphone::Core>& core,
                         const linphone::AccountParams& templateParams,
                         const config::v2::AccountPoolName& poolName,
                         const config::v2::AccountPool& pool,
                         std::unique_ptr<Loader>&& loader,
                         const GenericStruct* registrarConf)
    : mSuRoot{suRoot}, mCore{core}, mLoader{std::move(loader)} {
	const auto factory = linphone::Factory::get();
	const auto accountsDesc = mLoader->initialLoad();

	reserve(accountsDesc.size());
	for (const auto& accountDesc : accountsDesc) {
		if (accountDesc.uri.empty()) {
			LOGF("An account of account pool '%s' is missing a `uri` field", poolName.c_str());
		}
		const auto address = mCore->createAddress(accountDesc.uri);
		const auto accountParams = templateParams.clone();
		accountParams->setIdentityAddress(address);

		handleOutboundProxy(accountParams, accountDesc.outboundProxy);

		auto account = mCore->createAccount(accountParams);
		mCore->addAccount(account);

		if (!accountDesc.password.empty()) {
			mCore->addAuthInfo(factory->createAuthInfo(address->getUsername(), accountDesc.userid, accountDesc.password,
			                                           "", "", address->getDomain()));
		}

		try_emplace(accountDesc.uri, accountDesc.alias,
		            make_shared<Account>(account, pool.maxCallsPerLine, accountDesc.alias));

		if (registrarConf) {
			mRedisSub = make_unique<redis::async::SubscriptionSession>(
			    SoftPtr<redis::async::SessionListener>::fromObjectLivingLongEnough(*this));

			mRedisSub->connect(mSuRoot->getCPtr(), registrarConf->get<ConfigString>("redis-server-domain")->read(),
			                   registrarConf->get<ConfigInt>("redis-server-port")->read());
		}
	}
}

void AccountPool::handleOutboundProxy(const shared_ptr<linphone::AccountParams>& accountParams,
                                      const string& outboundProxy) {
	if (!outboundProxy.empty()) {
		// Override global pool config if outboundProxy is present
		const auto route = mCore->createAddress(outboundProxy);
		accountParams->setServerAddress(route);
		accountParams->setRoutesAddresses({route});
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

	try_emplaceAlias(alias, account);
}

void AccountPool::try_emplaceAlias(const string& alias, const shared_ptr<Account>& account) {
	if (alias.empty()) return;
	auto [__, isInsertedAlias] = mAccountsByAlias.try_emplace(alias, account);
	if (!isInsertedAlias) {
		SLOGE << "AccountPool::try_emplace uri[" << alias << "] already present, account only inserted by uri.";
	}
}

void AccountPool::accountUpdateNeeded(const RedisAccountPub& redisAccountPub) {
	OnAccountUpdateCB cb = [this](const config::v2::Account& accountToUpdate) {
		this->onAccountUpdate(accountToUpdate);
	};

	mLoader->accountUpdateNeeded(redisAccountPub, cb);
}

// TODO, how deletion work ?
void AccountPool::onAccountUpdate(config::v2::Account accountToUpdate) {
	auto accountByUriIt = mAccountsByUri.find(accountToUpdate.uri);
	auto accountByAliasIt = mAccountsByAlias.find(accountToUpdate.alias);
	if (accountByUriIt != mAccountsByUri.end() && accountByAliasIt != mAccountsByAlias.end()) {
		// Account update needed for password and/or outbound proxy only
		auto& updatedAccount = accountByUriIt->second;
		auto accountParams = updatedAccount->getLinphoneAccount()->getParams()->clone();

		handleOutboundProxy(accountParams, accountToUpdate.outboundProxy);
		updatedAccount->getLinphoneAccount()->setParams(accountParams);

		// TODO update password, AuthInfo in core ?
	} else if (accountByUriIt != mAccountsByUri.end()) {
		// + alias update
		auto& updatedAccount = accountByUriIt->second;
		auto accountParams = updatedAccount->getLinphoneAccount()->getParams()->clone();

		mAccountsByAlias.erase(updatedAccount->getAlias().str());
		updatedAccount->setAlias(accountToUpdate.alias);
		try_emplaceAlias(accountToUpdate.alias, updatedAccount);

		handleOutboundProxy(accountParams, accountToUpdate.outboundProxy);
		updatedAccount->getLinphoneAccount()->setParams(accountParams);

		// TODO update password, AuthInfo in core ?
	} else if (accountByUriIt != mAccountsByAlias.end()) {
		// + uri update
	} else {
		// new account
	}
}

void AccountPool::onConnect(int status) {
	if (status == REDIS_OK) {
		subscribeToAccountUpdate();
	} else {
		SLOGE << "AccountPool::onConnect : error while trying to connect to Redis. Status : " << status;
		// TODO tryReconnect();
	}
}

void AccountPool::subscribeToAccountUpdate() {
	auto* ready = mRedisSub->tryGetState<SubscriptionSession::Ready>();
	if (!ready) {
		return;
	}

	auto subscription = ready->subscriptions()["flexisip/B2BUA/account"];
	if (subscription.subscribed()) return;

	LOGD("Subscribing to account update ");
	subscription.subscribe([this](Reply reply) {
		try {
			auto replyAsString = std::get<reply::String>(reply);
			auto redisPub = nlohmann::json::parse(replyAsString).get<RedisAccountPub>();
			accountUpdateNeeded(redisPub);
		} catch (const std::bad_variant_access&) {
			// TODO reply .tostring ?
			SLOGE << "AccountPool::subscribeToAccountUpdate::callback : publish from redis not well formatted";
		}
		// TODO catch json exception here ?
	});
}

void AccountPool::onDisconnect(int status) {
	if (status != REDIS_OK) {
		SLOGE << "AccountPool::onDisconnect : disconnected from Redis. Status :" << status << ". Try reconnect ...";
		// TODO tryReconnect();
	}
}

} // namespace flexisip::b2bua::bridge