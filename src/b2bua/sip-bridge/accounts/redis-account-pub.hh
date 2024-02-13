/*
    Flexisip, a flexible SIP proxy server with media capabilities.
    Copyright (C) 2010-2024 Belledonne Communications SARL, All rights reserved.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <string>

#include "lib/nlohmann-json-3-11-2/json.hpp"

namespace flexisip::b2bua::bridge {

struct RedisAccountPub {
	std::string username;
	std::string domain;
	std::string identifier;
};

inline void from_json(const nlohmann ::json& nlohmann_json_j, RedisAccountPub& nlohmann_json_t) {
	NLOHMANN_JSON_FROM(username)
	NLOHMANN_JSON_FROM(domain)
	NLOHMANN_JSON_FROM(identifier)
}
} // namespace flexisip::b2bua::bridge