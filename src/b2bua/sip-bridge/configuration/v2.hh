/** Copyright (C) 2010-2024 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
    In-memory representation of a Provider configuration file
*/

#pragma once

#include <string>
#include <variant>
#include <vector>

#include "lib/nlohmann-json-3-11-2/json.hpp"

#include "media-encryption.hh"

#include "flexiapi/schemas/optional-json.hh"

namespace flexisip::b2bua::bridge::config::v2 {
namespace trigger_cond {

struct MatchRegex {
	std::string pattern;
	std::string source;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MatchRegex, pattern, source);

struct Always {};

} // namespace trigger_cond
namespace account_selection {

struct Random {};

struct FindInPool {
	std::string key;
	std::string source;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FindInPool, key, source);

} // namespace account_selection

using TriggerCondition = std::variant<trigger_cond::MatchRegex, trigger_cond::Always>;
inline void from_json(const nlohmann ::json& j, TriggerCondition& triggerCond) {
	using namespace trigger_cond;
	const auto strategy = j.at("strategy").get<std::string_view>();
	if (strategy == "MatchRegex") {
		triggerCond = j.get<MatchRegex>();
	} else if (strategy == "Always") {
		triggerCond = Always{};
	} else {
		throw std::runtime_error{
		    "Unknown 'triggerCondition/strategy' found in config. Supported strategies are 'MatchRegex' "
		    "and 'Always', not: " +
		    std::string(strategy)};
	}
}

using AccountToUse = std::variant<account_selection::Random, account_selection::FindInPool>;
inline void from_json(const nlohmann ::json& j, AccountToUse& accountToUse) {
	using namespace account_selection;
	const auto strategy = j.at("strategy").get<std::string_view>();
	if (strategy == "Random") {
		accountToUse = Random{};
	} else if (strategy == "FindInPool") {
		accountToUse = j.get<FindInPool>();
	} else {
		throw std::runtime_error{"Unknown 'accountToUse/strategy' found in config. Supported strategies are 'Random' "
		                         "and 'FindInPool', not: " +
		                         std::string(strategy)};
	}
}

using AccountPoolName = std::string;

enum struct OnAccountNotFound {
	NextProvider,
	Decline,
};
NLOHMANN_JSON_SERIALIZE_ENUM(OnAccountNotFound,
                             {
                                 {OnAccountNotFound::NextProvider, "nextProvider"},
                                 {OnAccountNotFound::Decline, "decline"},
                             })

struct OutgoingInvite {
	std::string to;
	std::string from;
};
inline void from_json(const nlohmann ::json& nlohmann_json_j, OutgoingInvite& nlohmann_json_t) {
	OutgoingInvite nlohmann_json_default_obj;
	NLOHMANN_JSON_FROM(to)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(from)
};

struct Provider {
	std::string name;
	std::string outboundProxy;
	bool registrationRequired;
	uint32_t maxCallsPerLine;
	AccountPoolName accountPool;
	TriggerCondition triggerCondition;
	AccountToUse accountToUse;
	OnAccountNotFound onAccountNotFound;
	OutgoingInvite outgoingInvite;
	std::optional<bool> enableAvpf;
	std::optional<linphone::MediaEncryption> mediaEncryption;
};
inline void from_json(const nlohmann ::json& nlohmann_json_j, Provider& nlohmann_json_t) {
	Provider nlohmann_json_default_obj;
	NLOHMANN_JSON_FROM(name)
	NLOHMANN_JSON_FROM(outboundProxy)
	NLOHMANN_JSON_FROM(registrationRequired)
	NLOHMANN_JSON_FROM(maxCallsPerLine)
	NLOHMANN_JSON_FROM(accountPool)
	from_json(nlohmann_json_j.at("triggerCondition"), nlohmann_json_t.triggerCondition);
	from_json(nlohmann_json_j.at("accountToUse"), nlohmann_json_t.accountToUse);
	NLOHMANN_JSON_FROM(onAccountNotFound)
	NLOHMANN_JSON_FROM(outgoingInvite)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(enableAvpf)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(mediaEncryption)
}

struct Account {
	std::string uri;
	std::string userid;
	std::string password;
	std::string alias;
};
inline void from_json(const nlohmann ::json& nlohmann_json_j, Account& nlohmann_json_t) {
	Account nlohmann_json_default_obj;
	NLOHMANN_JSON_FROM(uri)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(userid)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(password)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(alias)
};

struct SQLPool {
	std::string initQuery;
	std::string updateQuery;
	std::string connection;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SQLPool, initQuery, updateQuery, connection);

using StaticPool = std::vector<Account>;

using AccountPool = std::variant<StaticPool, SQLPool>;
inline void from_json(const nlohmann ::json& j, AccountPool& pool) {
	if (j.is_array()) {
		pool = j.get<StaticPool>();
	} else {
		pool = j.get<SQLPool>();
	}
}

struct Root {
	unsigned int schemaVersion;
	std::vector<Provider> providers;
	std::unordered_map<AccountPoolName, AccountPool> accountPools;
};
inline void from_json(const nlohmann ::json& nlohmann_json_j, Root& nlohmann_json_t) {
	NLOHMANN_JSON_FROM(schemaVersion)
	NLOHMANN_JSON_FROM(providers)
	NLOHMANN_JSON_FROM(accountPools)
}

} // namespace flexisip::b2bua::bridge::config::v2