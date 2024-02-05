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
using namespace utils::string_interpolation;

namespace {
using namespace variable_substitution;

Substituter<const linphone::Call&, const Account&> resolver(const std::string_view variableName) {
	const auto [varName, furtherPath] = popVarName(variableName);

	if (varName == "incoming") {
		return resolve([](const auto& call, const auto&) -> const linphone::Call& { return call; },
		               linphone_call::kFields)(furtherPath);
	} else if (varName == "account") {
		return resolve([](const auto&, const auto& account) -> const Account& { return account; },
		               account::kFields)(furtherPath);
	}
	throw std::runtime_error{"unimplemented"};
};

} // namespace

InviteTweaker::InviteTweaker(const config::v2::OutgoingInvite& config, linphone::Core& core)
    : mToHeader(InterpolatedString(config.to, "{", "}"), resolver),
      mFromHeader(config.from.empty()
                      ? std::nullopt
                      : decltype(mFromHeader)(StringTemplate(InterpolatedString(config.from, "{", "}"), resolver))),
      mOutboundProxyOverride(config.outboundProxy ? core.createAddress(*config.outboundProxy) : nullptr),
      mAvpfOverride(config.enableAvpf), mEncryptionOverride(config.mediaEncryption) {
}

std::shared_ptr<linphone::Address> InviteTweaker::tweakInvite(const linphone::Call& incomingCall,
                                                              const Account& account,
                                                              linphone::CallParams& outgoingCallParams) const {
	auto linphoneAccount = account.getLinphoneAccount();
	if (mOutboundProxyOverride) {
		const auto& accountParams = linphoneAccount->getParams()->clone();
		accountParams->setServerAddress(mOutboundProxyOverride);
		accountParams->setRoutesAddresses({mOutboundProxyOverride});
		linphoneAccount = linphoneAccount->getCore()->createAccount(accountParams);
	}
	outgoingCallParams.setAccount(linphoneAccount);

	if (const auto& mediaEncryption = mEncryptionOverride) {
		outgoingCallParams.setMediaEncryption(*mediaEncryption);
	}
	if (const auto& enableAvpf = mAvpfOverride) {
		outgoingCallParams.enableAvpf(*enableAvpf);
	}

	if (mFromHeader) {
		outgoingCallParams.setFromHeader(mFromHeader->format(incomingCall, account));
	}
	return incomingCall.getCore()->createAddress(mToHeader.format(incomingCall, account));
}

} // namespace flexisip::b2bua::bridge
