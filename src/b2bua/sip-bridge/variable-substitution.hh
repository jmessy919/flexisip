/** Copyright (C) 2010-2024 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <string>

#include <linphone++/address.hh>
#include <linphone++/call.hh>

#include "b2bua/sip-bridge/accounts/account.hh"
#include "flexisip/utils/sip-uri.hh"

#include "utils/string-utils.hh"

namespace flexisip::b2bua::bridge::variable_substitution {

template <typename TContext>
using Resolver = std::string (*)(const TContext&, std::string_view);

template <typename TContext>
struct FieldOf {
	const std::string_view name;
	const Resolver<TContext> resolver;
};

template <typename TContext>
using FieldsOf = std::initializer_list<const FieldOf<TContext>>;

inline std::pair<std::string_view, std::string_view> popVarName(std::string_view dotPath) {
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

static const FieldsOf<std::shared_ptr<const linphone::Address>> kFields = {
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

inline std::string resolve(const std::shared_ptr<const linphone::Address>& address, std::string_view varName) {
	return variable_substitution::resolve(kFields, address, varName);
}

} // namespace linphone_address

namespace linphone_call {

static const FieldsOf<linphone::Call> kFields = {
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

namespace sofia_uri {

static const FieldsOf<SipUri> kFields = {
    {"", [](const auto& uri, const auto) { return uri.str(); }},
    {"user", [](const auto& uri, const auto) { return uri.getUser(); }},
    {"hostport",
     [](const auto& uri, const auto) {
	     auto hostport = uri.getHost();
	     if (const auto port = uri.getPort(); port != "") {
		     hostport += ":" + port;
	     }
	     return hostport;
     }},
    {"uriParameters",
     [](const auto& uri, const auto) {
	     auto params = uri.getParams();
	     if (!params.empty()) {
		     params = ";" + params;
	     }
	     return params;
     }},
};

}

namespace account {

static const FieldsOf<Account> kFields = {
    {"sipIdentity",
     [](const auto& account, const auto furtherPath) {
	     return linphone_address::resolve(account.getLinphoneAccount()->getParams()->getIdentityAddress(), furtherPath);
     }},
    {"alias", [](const auto& account,
                 const auto furtherPath) { return resolve(sofia_uri::kFields, account.getAlias(), furtherPath); }},
};

} // namespace account

} // namespace flexisip::b2bua::bridge::variable_substitution