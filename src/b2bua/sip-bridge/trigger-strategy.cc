/** Copyright (C) 2010-2024 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "trigger-strategy.hh"

#include <regex>

#include <linphone++/address.hh>

namespace flexisip::b2bua::bridge::trigger_strat {

MatchRegex::MatchRegex(conf::MatchRegex&& config) : mPattern(std::move(config.pattern)) {
}

bool MatchRegex::shouldHandleThisCall(const linphone::Call& call) {
	return std::regex_match(call.getRequestAddress()->asStringUriOnly(), mPattern);
}

} // namespace flexisip::b2bua::bridge::trigger_strat
