/** Copyright (C) 2010-2024 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <string>
#include <variant>

#include <linphone++/address.hh>
#include <linphone++/call.hh>

#include "flexisip/utils/sip-uri.hh"

#include "b2bua/sip-bridge/accounts/account.hh"
#include "utils/string-interpolation/exceptions.hh"
#include "utils/string-utils.hh"

namespace flexisip::b2bua::bridge::variable_substitution {

template <typename... Args>
using Substituter = std::function<std::string(const Args&...)>;

template <typename... Args>
using Resolver = std::function<Substituter<Args...>(std::string_view)>;

template <typename... Args>
using FieldsOf = std::unordered_map<std::string_view, Resolver<Args...>>;

template <typename TSubstituter>
constexpr auto leaf(TSubstituter substituter) {
	return [substituter](std::string_view furtherPath) {
		if (furtherPath != "") {
			throw utils::string_interpolation::ContextlessResolutionError(furtherPath);
		}

		return substituter;
	};
}

inline std::pair<std::string_view, std::string_view> popVarName(std::string_view dotPath) {
	const auto split = StringUtils::splitOnce(dotPath, ".");
	if (!split) return {dotPath, ""};

	const auto [head, tail] = *split;
	return {head, tail};
}

// TODO: specialize (restrict) types
template <typename Transformer, typename TFields>
constexpr auto resolve(Transformer transformer, const TFields& fields) {
	return [transformer, &fields](const auto dotPath) {
		const auto [varName, furtherPath] = popVarName(dotPath);
		const auto resolver = fields.find(varName);
		if (resolver == fields.end()) {
			throw utils::string_interpolation::ContextlessResolutionError(varName);
		}

		const auto substituter = resolver->second(furtherPath);

		return [substituter, transformer](const auto&... args) { return substituter(transformer(args...)); };
	};
}

namespace linphone_address {

static const FieldsOf<std::shared_ptr<const linphone::Address>> kFields = {
    {"", leaf([](const auto& address) { return address->asStringUriOnly(); })},
    {"user", leaf([](const std::shared_ptr<const linphone::Address>& address) { return address->getUsername(); })},
    {"hostport", leaf([](const auto& address) {
	     auto hostport = address->getDomain();
	     const auto port = address->getPort();
	     if (port != 0) {
		     hostport += ":" + std::to_string(port);
	     }
	     return hostport;
     })},
    {"uriParameters", leaf([](const auto& address) {
	     auto params = SipUri{address->asStringUriOnly()}.getParams();
	     if (!params.empty()) {
		     params = ";" + params;
	     }
	     return params;
     })},
};

} // namespace linphone_address

namespace linphone_call {

static const FieldsOf<linphone::Call> kFields = {
    {"to", resolve([](const auto& call) { return call.getToAddress(); }, linphone_address::kFields)},
    {"from", resolve([](const auto& call) { return call.getRemoteAddress(); }, linphone_address::kFields)},
    {"requestAddress", resolve([](const auto& call) { return call.getRequestAddress(); }, linphone_address::kFields)},
};

} // namespace linphone_call

namespace sofia_uri {

static const FieldsOf<SipUri> kFields = {
    {"", leaf([](const auto& uri) { return uri.str(); })},
    {"user", leaf([](const auto& uri) { return uri.getUser(); })},
    {"hostport", leaf([](const auto& uri) {
	     auto hostport = uri.getHost();
	     if (const auto port = uri.getPort(); port != "") {
		     hostport += ":" + port;
	     }
	     return hostport;
     })},
    {"uriParameters", leaf([](const auto& uri) {
	     auto params = uri.getParams();
	     if (!params.empty()) {
		     params = ";" + params;
	     }
	     return params;
     })},
};

}

namespace account {

static const FieldsOf<Account> kFields = {
    {"sipIdentity",
     resolve([](const auto& account) { return account.getLinphoneAccount()->getParams()->getIdentityAddress(); },
             linphone_address::kFields)},
    {"alias", resolve([](const auto& account) { return account.getAlias(); }, sofia_uri::kFields)},
};

}
} // namespace flexisip::b2bua::bridge::variable_substitution