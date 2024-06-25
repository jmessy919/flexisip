/** Copyright (C) 2010-2024 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

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

/**
 * @brief Builds a leaf Resolver that does not accept any sub fields
 *
 * @param substituter the substitution function for this field
 */
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

/**
 * Builds a (sub-)Resolver from a transformation function and fields map
 */
template <typename Transformer = std::nullopt_t, typename... Context>
class FieldsResolver {
public:
	using Fields = FieldsOf<Context...>;

	auto operator()(std::string_view dotPath) const {
		const auto& [varName, furtherPath] = popVarName(dotPath);
		const auto& resolver = fields.find(varName);
		if (resolver == fields.end()) {
			throw utils::string_interpolation::ContextlessResolutionError(varName);
		}

		const auto& substituter = resolver->second(furtherPath);

		return [substituter, transformer = this->transformer](const auto&... args) {
			if constexpr (!std::is_same_v<Transformer, std::nullopt_t>) {
				return substituter(transformer(args...));
			} else {
				return substituter(args...);
				std::ignore = transformer; // Suppress unused warning
			}
		};
	}

	// Available fields in this resolution context
	const Fields& fields;
	// Callable to extract a new sub-context (field) from the current context
	Transformer transformer = std::nullopt;
};

template <typename... Context>
FieldsResolver(const FieldsOf<Context...>&) -> FieldsResolver<std::nullopt_t, Context...>;
template <typename... Context, typename Transformer>
FieldsResolver(const FieldsOf<Context...>&, Transformer) -> FieldsResolver<Transformer, Context...>;

const auto kLinphoneAddressFields = FieldsOf<std::shared_ptr<const linphone::Address>>{
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

const auto kLinphoneCallFields = FieldsOf<linphone::Call>{
    {"to", FieldsResolver{kLinphoneAddressFields, [](const auto& call) { return call.getToAddress(); }}},
    {"from", FieldsResolver{kLinphoneAddressFields, [](const auto& call) { return call.getRemoteAddress(); }}},
    {"requestUri", FieldsResolver{kLinphoneAddressFields, [](const auto& call) { return call.getRequestAddress(); }}},
};

const auto kSofiaUriFields = FieldsOf<SipUri>{
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

const auto kAccountFields = FieldsOf<Account>{
    {"uri", FieldsResolver{kLinphoneAddressFields,
                           [](const auto& account) {
	                           return account.getLinphoneAccount()->getParams()->getIdentityAddress();
                           }}},
    {"alias", FieldsResolver{kSofiaUriFields, [](const auto& account) { return account.getAlias(); }}},
};

} // namespace flexisip::b2bua::bridge::variable_substitution

namespace std {

// Same as hash<nullptr_t>
template <>
struct hash<nullopt_t> {
	size_t operator()(const nullopt_t&) const {
		return 0;
	}
};

template <typename Transformer, typename... Context>
struct hash<flexisip::b2bua::bridge::variable_substitution::FieldsResolver<Transformer, Context...>> {
	using Target = flexisip::b2bua::bridge::variable_substitution::FieldsResolver<Transformer, Context...>;
	size_t operator()(const Target& resolver) const {
		return hash<const typename Target::Fields*>()(addressof(resolver.fields)) ^
		       hash<Transformer>()(resolver.transformer);
	}
};
} // namespace std