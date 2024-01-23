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

std::pair<std::string_view, std::string_view> popVarName(std::string_view dotPath) {
	const auto split = StringUtils::splitOnce(dotPath, ".");
	if (!split) return {dotPath, ""};

	const auto [head, tail] = *split;
	return {head, tail};
}

class LinphoneAddress {
public:
	using SubResolver = std::string (*)(const std::shared_ptr<const linphone::Address>&);

	constexpr static std::initializer_list<std::pair<std::string_view, SubResolver>> kFields = {
	    {"", [](const auto& address) { return address->asStringUriOnly(); }},
	    {"user", [](const auto& address) { return address->getUsername(); }},
	    {"hostport",
	     [](const auto& address) {
		     auto hostport = address->getDomain();
		     const auto port = address->getPort();
		     if (port != 0) {
			     hostport += ":" + std::to_string(port);
		     }
		     return hostport;
	     }},
	    {"uriParameters",
	     [](const auto& address) {
		     auto params = SipUri{address->asStringUriOnly()}.getParams();
		     if (!params.empty()) {
			     params = ";" + params;
		     }
		     return params;
	     }},
	};

	explicit LinphoneAddress(std::shared_ptr<const linphone::Address> address) : mAddress(address) {
	}

	std::string resolve(std::string_view varName) const {
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

namespace linphone_call {

using SubResolver = std::string (*)(const linphone::Call&, std::string_view);

constexpr std::initializer_list<std::pair<std::string_view, SubResolver>> kFields = {
    {"to", [](const auto& call,
              const auto furtherPath) { return LinphoneAddress(call.getToAddress()).resolve(furtherPath); }},
    {"from", [](const auto& call,
                const auto furtherPath) { return LinphoneAddress(call.getRemoteAddress()).resolve(furtherPath); }},
    {"requestAddress",
     [](const auto& call, const auto furtherPath) {
	     return LinphoneAddress(call.getRequestAddress()).resolve(furtherPath);
     }},
};

std::string resolve(const linphone::Call& call, std::string_view dotPath) {
	const auto [varName, furtherPath] = variable_resolution::popVarName(dotPath);
	for (const auto& [name, resolver] : kFields) {
		if (name == varName) {
			return resolver(call, furtherPath);
		}
	}
	throw std::runtime_error{"unsupported variable name"};
}

} // namespace linphone_call

namespace account {

using SubResolver = std::string (*)(const Account&, std::string_view);

constexpr std::initializer_list<std::pair<std::string_view, SubResolver>> kFields = {
    {"sipIdentity",
     [](const auto& account, const auto furtherPath) {
	     return LinphoneAddress(account.getLinphoneAccount()->getParams()->getIdentityAddress()).resolve(furtherPath);
     }},
    {"alias", [](const auto& account, const auto) { return account.getAlias(); }},
};

std::string resolve(const Account& account, std::string_view dotPath) {
	const auto [varName, furtherPath] = variable_resolution::popVarName(dotPath);
	for (const auto& [name, resolver] : kFields) {
		if (name == varName) {
			return resolver(account, furtherPath);
		}
	}
	throw std::runtime_error{"unsupported variable name"};
}

} // namespace account
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
		const auto [varName, furtherPath] = variable_resolution::popVarName(variableName);

		if (varName == "incoming") {
			return variable_resolution::linphone_call::resolve(incomingCall, furtherPath);
		} else if (varName == "account") {
			return variable_resolution::account::resolve(account, furtherPath);
		}
		throw std::runtime_error{"unimplemented"};
	}};
	if (mFromHeader) {
		outgoingCallParams.setFromHeader(mFromHeader->format(variableResolver));
	}
	return incomingCall.getCore()->createAddress(mToHeader.format(variableResolver));
}

} // namespace flexisip::b2bua::bridge
