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

#include <memory>

#include "linphone++/linphone.hh"

#pragma once

namespace flexisip::b2bua::bridge {

class Account {
	friend class SipBridge;
	friend class ExternalSipProvider;
	friend class InviteTweaker;

public:
	Account(const std::shared_ptr<linphone::Account>& account, uint16_t freeSlots, std::string_view alias);

	// Move constructor
	Account(Account&& other) = default;

	bool isAvailable() const;

private:
	// Disable copy semantics
	Account(const Account&) = delete;
	Account& operator=(const Account&) = delete;

	std::shared_ptr<linphone::Account> account;
	uint16_t freeSlots = 0;
	std::string mAlias{};
};

} // namespace flexisip::b2bua::bridge
