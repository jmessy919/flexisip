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
namespace variable_resolution {

class Address {
public:
	using SubResolver = std::string (*)(const std::shared_ptr<const linphone::Address>&);

	constexpr static std::pair<std::string_view, SubResolver> kFields[] = {
	    {"user", [](const auto& address) { return address->getUsername(); }},
	    {"uriParameters",
	     [](const auto& address) {
		     auto params = SipUri{address->asStringUriOnly()}.getParams();
		     if (!params.empty()) {
			     params = ";" + params;
		     }
		     return params;
	     }},
	};

	explicit Address(std::shared_ptr<const linphone::Address> address) : mAddress(address) {
	}

	std::string resolve(std::string_view varName) {
		for (const auto& [name, resolver] : kFields) {
			if (name == varName) {
				return resolver(mAddress);
			}
		}
		throw std::runtime_error{"unsupported variable name"};
	}

private:
	std::shared_ptr<const linphone::Address> mAddress;
};

} // namespace variable_resolution

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
		const auto dotPath = StringUtils::split(std::string_view(variableName), ".");
		if (dotPath[0] == "incoming") {
			if (dotPath[1] == "to") {
				return incomingCall.getToAddress()->asStringUriOnly();
			} else if (dotPath[1] == "from") {
				return incomingCall.getRemoteAddress()->asStringUriOnly();
			} else if (dotPath[1] == "requestAddress") {
				return variable_resolution::Address(incomingCall.getRequestAddress()).resolve(dotPath[2]);
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
	if (mFromHeader) {
		outgoingCallParams.setFromHeader(mFromHeader->format(variableResolver));
	}
	return incomingCall.getCore()->createAddress(mToHeader.format(variableResolver));
}

} // namespace flexisip::b2bua::bridge
