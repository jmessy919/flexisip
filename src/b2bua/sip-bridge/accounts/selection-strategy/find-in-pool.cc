/** Copyright (C) 2010-2024 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "find-in-pool.hh"

#include "b2bua/sip-bridge/variable-substitution.hh"

namespace flexisip::b2bua::bridge::account_strat {

FindInPool::FindInPool(std::shared_ptr<AccountPool> accountPool,
                       const config::v2::account_selection::FindInPool& config)
    : AccountSelectionStrategy(accountPool), mLookUpField(config.by), mSource(config.source) {
}

std::shared_ptr<Account> FindInPool::chooseAccountForThisCall(const linphone::Call& incomingCall) const {
	const auto& pool = *getAccountPool();
	StringFormatter::TranslationFunc variableResolver{[&incomingCall](const std::string& varName) {
		using namespace variable_substitution;
		return resolve(linphone_call::kFields, incomingCall, varName);
	}};
	const auto source = mSource.format(variableResolver);

	switch (mLookUpField) {
		using namespace config::v2::account_selection;
		case AccountLookUp::ByUri: {
			return pool.getAccountByUri(source);
		} break;
		case AccountLookUp::ByAlias: {
			return pool.getAccountByAlias(source);
		} break;
		default: {
			throw std::logic_error{"Missing case statement"};
		} break;
	};
}

} // namespace flexisip::b2bua::bridge::account_strat
