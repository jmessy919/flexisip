/** Copyright (C) 2010-2024 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "invite-tweaker.hh"

#include <linphone++/account_params.hh>
#include <linphone++/address.hh>
#include <linphone++/call_params.hh>
#include <linphone++/core.hh>

#include "b2bua/sip-bridge/variable-substitution.hh"

namespace flexisip::b2bua::bridge {

InviteTweaker::InviteTweaker(const config::v2::OutgoingInvite& config)
    : mToHeader(config.to), mFromHeader(config.from.empty() ? std::nullopt : decltype(mFromHeader){config.from}),
      mAvpfOverride(config.enableAvpf), mEncryptionOverride(config.mediaEncryption) {
}

std::shared_ptr<linphone::Address> InviteTweaker::tweakInvite(const linphone::Call& incomingCall,
                                                              const Account& account,
                                                              linphone::CallParams& outgoingCallParams) const {

	if (const auto& mediaEncryption = mEncryptionOverride) {
		outgoingCallParams.setMediaEncryption(*mediaEncryption);
	}
	if (const auto& enableAvpf = mAvpfOverride) {
		outgoingCallParams.enableAvpf(*enableAvpf);
	}

	StringFormatter::TranslationFunc variableResolver{[&incomingCall, &account](const std::string& variableName) {
		using namespace variable_substitution;
		const auto [varName, furtherPath] = popVarName(variableName);

		if (varName == "incoming") {
			return resolve(linphone_call::kFields, incomingCall, furtherPath);
		} else if (varName == "account") {
			return resolve(account::kFields, account, furtherPath);
		}
		throw std::runtime_error{"unimplemented"};
	}};
	if (mFromHeader) {
		outgoingCallParams.setFromHeader(mFromHeader->format(variableResolver));
	}
	return incomingCall.getCore()->createAddress(mToHeader.format(variableResolver));
}

} // namespace flexisip::b2bua::bridge
