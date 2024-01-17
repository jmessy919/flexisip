/** Copyright (C) 2010-2024 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "invite-tweaker.hh"

#include <linphone++/account_params.hh>
#include <linphone++/address.hh>
#include <linphone++/call_params.hh>
#include <linphone++/core.hh>

#include "flexisip/utils/sip-uri.hh"

namespace flexisip::b2bua::bridge {

InviteTweaker::InviteTweaker(const config::v2::OutgoingInvite& config)
    : mToHeader(config.to), mAvpfOverride(config.enableAvpf), mEncryptionOverride(config.mediaEncryption) {
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
		const auto dotPath = StringUtils::split(std::string_view(variableName), ".");
		if (dotPath[0] == "incoming") {
			if (dotPath[1] == "to") {
				return incomingCall.getToAddress()->asStringUriOnly();
			} else if (dotPath[1] == "requestAddress") {
				const auto& requestAddress = *incomingCall.getRequestAddress();
				if (dotPath[2] == "user") {
					return requestAddress.getUsername();
				} else if (dotPath[2] == "uriParameters") {
					auto params = SipUri{requestAddress.asStringUriOnly()}.getParams();
					if (!params.empty()) {
						params = ";" + params;
					}
					return params;
				}
			}
		} else if (dotPath[0] == "account") {
			if (dotPath[1] == "sipIdentity") {
				const auto sipIdentity = account.account->getParams()->getIdentityAddress();
				if (dotPath[2] == "hostport") {
					auto hostport = sipIdentity->getDomain();
					const auto port = sipIdentity->getPort();
					if (port != 0) {
						hostport += ":" + std::to_string(port);
					}
					return hostport;
				}
			}
		}
		throw std::runtime_error{"unimplemented"};
	}};
	return incomingCall.getCore()->createAddress(mToHeader.format(variableResolver));
}

} // namespace flexisip::b2bua::bridge
