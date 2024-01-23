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

template <typename TContext>
using Resolver = std::string (*)(const TContext&, std::string_view);

template <typename TContext>
using FieldsOf = std::initializer_list<std::pair<std::string_view, Resolver<TContext>>>;

std::pair<std::string_view, std::string_view> popVarName(std::string_view dotPath) {
	const auto split = StringUtils::splitOnce(dotPath, ".");
	if (!split) return {dotPath, ""};

	const auto [head, tail] = *split;
	return {head, tail};
}

template <typename TContext>
std::string resolve(const FieldsOf<TContext>& fields, const TContext& context, std::string_view dotPath) {
	const auto [varName, furtherPath] = popVarName(dotPath);
	for (const auto& [name, resolver] : fields) {
		if (name == varName) {
			return resolver(context, furtherPath);
		}
	}
	throw std::runtime_error{"unsupported variable name"};
}

namespace linphone_address {

constexpr static FieldsOf<std::shared_ptr<const linphone::Address>> kFields = {
    {"", [](const auto& address, const auto) { return address->asStringUriOnly(); }},
    {"user", [](const auto& address, const auto) { return address->getUsername(); }},
    {"hostport",
     [](const auto& address, const auto) {
	     auto hostport = address->getDomain();
	     const auto port = address->getPort();
	     if (port != 0) {
		     hostport += ":" + std::to_string(port);
	     }
	     return hostport;
     }},
    {"uriParameters",
     [](const auto& address, const auto) {
	     auto params = SipUri{address->asStringUriOnly()}.getParams();
	     if (!params.empty()) {
		     params = ";" + params;
	     }
	     return params;
     }},
};

std::string resolve(const std::shared_ptr<const linphone::Address>& address, std::string_view varName) {
	return variable_resolution::resolve(kFields, address, varName);
}

} // namespace linphone_address

namespace linphone_call {

constexpr FieldsOf<linphone::Call> kFields = {
    {"to", [](const auto& call,
              const auto furtherPath) { return linphone_address::resolve(call.getToAddress(), furtherPath); }},
    {"from", [](const auto& call,
                const auto furtherPath) { return linphone_address::resolve(call.getRemoteAddress(), furtherPath); }},
    {"requestAddress",
     [](const auto& call, const auto furtherPath) {
	     return linphone_address::resolve(call.getRequestAddress(), furtherPath);
     }},
};

} // namespace linphone_call

namespace account {

constexpr FieldsOf<Account> kFields = {
    {"sipIdentity",
     [](const auto& account, const auto furtherPath) {
	     return linphone_address::resolve(account.getLinphoneAccount()->getParams()->getIdentityAddress(), furtherPath);
     }},
    {"alias", [](const auto& account, const auto) { return account.getAlias(); }},
};

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
		using namespace variable_resolution;
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
