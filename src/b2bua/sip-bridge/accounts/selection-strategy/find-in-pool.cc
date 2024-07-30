/** Copyright (C) 2010-2024 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "find-in-pool.hh"

#include "b2bua/sip-bridge/string-format-fields.hh"

namespace flexisip::b2bua::bridge::account_strat {
using namespace utils::string_interpolation;
using namespace std::string_literals;

FindInPool::FindInPool(std::shared_ptr<AccountPool> accountPool,
                       const config::v2::account_selection::FindInPool& config)
    : AccountSelectionStrategy(accountPool), mAccountLookup(mAccountPool->getOrCreateView({
                                                 InterpolatedString(
                                                     [&] {
	                                                     // Backward compat.: This field was previously an enum of "uri"
	                                                     // | "alias"
	                                                     if (config.by == "uri") return "{uri}"s;
	                                                     else if (config.by == "alias") return "{alias}"s;
	                                                     else return config.by;
                                                     }(),
                                                     "{",
                                                     "}"),
                                                 resolve(kAccountFields),
                                             })),
      mSourceTemplate(InterpolatedString(config.source, "{", "}"), resolve(kLinphoneCallFields)) {
}

std::shared_ptr<Account> FindInPool::chooseAccountForThisCall(const linphone::Call& incomingCall) const {
	const auto& source = mSourceTemplate.format(incomingCall);

	const auto& [interpolator, view] = mAccountLookup;
	auto log = pumpstream(FLEXISIP_LOG_DOMAIN, BCTBX_LOG_DEBUG);
	log << "FindInPool strategy attempted to find an account matching " << interpolator.getTemplate() << " == '"
	    << source << "' for call '" << incomingCall.getCallLog()->getCallId() << "': ";

	const auto maybeAccount = view.find(source);
	if (maybeAccount == view.end()) {
		log << "not found";
		return {};
	}

	const auto& account = maybeAccount->second;
	log << "found '" << account->getLinphoneAccount()->getParams()->getIdentityAddress()->asString();
	return account;
}

} // namespace flexisip::b2bua::bridge::account_strat
