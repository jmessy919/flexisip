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

#include <string>

#include "lib/nlohmann-json-3-11-2/json.hpp"

#pragma once

namespace flexisip {

class MessageDeviceResponse {
public:
	MessageDeviceResponse() = default;
	explicit MessageDeviceResponse(int lastStatus, const std::string& receivedAt)
	    : last_status(lastStatus), received_at(receivedAt) {
	}
	NLOHMANN_DEFINE_TYPE_INTRUSIVE(MessageDeviceResponse, last_status, received_at)

private:
	int last_status = 0;
	std::string received_at{};
};

} // namespace flexisip
