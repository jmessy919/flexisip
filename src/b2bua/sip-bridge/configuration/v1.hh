/** Copyright (C) 2010-2024 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
    In-memory representation of a Provider configuration file
*/

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "lib/nlohmann-json-3-11-2/json.hpp"

#include "linphone++/enums.hh"

#include "flexiapi/schemas/optional-json.hh" // IWYU pragma: keep. To serialize std::optional<T>

namespace flexisip::b2bua::bridge::config::v1 {

struct AccountDesc {
	std::string uri;
	std::string userid;
	std::string password;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AccountDesc, uri, userid, password);

struct ProviderDesc {
	std::string name;
	std::string pattern;
	std::string outboundProxy;
	bool registrationRequired;
	uint32_t maxCallsPerLine;
	std::vector<AccountDesc> accounts;
	std::optional<bool> overrideAvpf;
	std::optional<linphone::MediaEncryption> overrideEncryption;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ProviderDesc,
                                                name,
                                                pattern,
                                                outboundProxy,
                                                registrationRequired,
                                                maxCallsPerLine,
                                                accounts,
                                                overrideAvpf,
                                                overrideEncryption);

using Root = std::vector<ProviderDesc>;

} // namespace flexisip::b2bua::bridge::config::v1
