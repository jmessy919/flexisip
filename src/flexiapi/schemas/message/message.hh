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

#include "flexiapi/schemas/optional-json.hh"
#include "lib/nlohmann-json-3-11-2/json.hpp"
#include "message-device-response.hh"

#pragma once

namespace flexisip {

using MessageDevices = std::unordered_map<std::string, std::optional<MessageDeviceResponse>>;
using To = std::unordered_map<std::string, MessageDevices>;

class Message {
	friend class FlexiStats;
public:
	Message() = default;
	Message(const std::string& id,
	        const std::string& from,
	        const To& to,
	        const std::string& sentAt,
	        bool encrypted,
	        const std::optional<std::string>& conferenceId)
	    : id(id), from(from), to(to), sent_at(sentAt), encrypted(encrypted), conference_id(conferenceId) {
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(Message, id, from, to, sent_at, encrypted, conference_id);

private:
	std::string id;
	std::string from;
	To to;
	std::string sent_at;
	bool encrypted;
	std::optional<std::string> conference_id;
};

} // namespace flexisip