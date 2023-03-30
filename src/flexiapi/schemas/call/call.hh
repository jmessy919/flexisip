/*
    Flexisip, a flexible SIP proxy server with media capabilities.
    Copyright (C) 2010-2023 Belledonne Communications SARL, All rights reserved.

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

#include <optional>
#include <string>
#include <unordered_map>

#include "call-device-state.hh"
#include "flexiapi/schemas/optional-json.hh"
#include "lib/nlohmann-json-3-11-2/json.hpp"
#include "terminated.hh"

#pragma once

namespace flexisip {

using CallDevices = std::unordered_map<std::string, std::optional<CallDeviceState>>;

class Call {
	friend class FlexiStats;

public:
	Call() = default;
	Call(const std::string& id,
	     const std::string& from,
	     const std::string& to,
	     const CallDevices& devices,
	     const std::string& initiatedAt,
	     const std::optional<std::string>& endedAt,
	     const std::optional<std::string>& conferenceId)
	    : id(id), from(from), to(to), devices(devices), initiated_at(initiatedAt), ended_at(endedAt),
	      conference_id(conferenceId) {
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(Call, id, from, to,devices, initiated_at, ended_at, conference_id);

private:
	std::string id;
	std::string from;
	std::string to;
	CallDevices devices;
	std::string initiated_at;
	std::optional<std::string> ended_at;
	std::optional<std::string> conference_id;
};

} // namespace flexisip