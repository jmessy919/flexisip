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

#include "linphone++/enums.hh"

namespace flexisip::b2bua::bridge::config::v1 {

struct AccountDesc {
	std::string uri;
	std::string userid;
	std::string password;
};

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

using Root = std::vector<ProviderDesc>;

} // namespace flexisip::b2bua::bridge::configuration::v1
