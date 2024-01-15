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
/*
    Tools to bridge calls to other SIP providers via the Back-to-Back User Agent
    E.g. for placing PSTN calls

    NOT thread safe
*/

#pragma once

#include <optional>
#include <regex>
#include <unordered_map>

#include "linphone++/enums.hh"
#include "linphone++/linphone.hh"

#include "b2bua/b2bua-server.hh"
#include "b2bua/sip-bridge/accounts/account.hh"
#include "b2bua/sip-bridge/trigger-strategy.hh"
#include "cli.hh"
#include "configuration/v2.hh"

namespace flexisip::b2bua::bridge {

class ExternalSipProvider {
	friend class SipBridge;

public:
	// Move constructor
	ExternalSipProvider(ExternalSipProvider&& other) = default;

	std::optional<b2bua::Application::ActionToTake>
	onCallCreate(const linphone::Call& incomingCall,
	             linphone::CallParams& outgoingCallParams,
	             std::unordered_map<std::string, Account*>& occupiedSlots);

private:
	ExternalSipProvider(std::unique_ptr<trigger_strat::TriggerStrategy>&& triggerStrat,
	                    std::vector<Account>&& accounts,
	                    std::string&& name,
	                    const std::optional<bool>& overrideAvpf,
	                    const std::optional<linphone::MediaEncryption>& overrideEncryption);

	Account* findAccountToMakeTheCall();

	std::unique_ptr<trigger_strat::TriggerStrategy> mTriggerStrat;
	config::v2::OnAccountNotFound mOnAccountNotFound;
	std::vector<Account> accounts;
	std::string name;
	std::optional<bool> overrideAvpf;
	std::optional<linphone::MediaEncryption> overrideEncryption;

	// Disable copy semantics
	ExternalSipProvider(const ExternalSipProvider&) = delete;
	ExternalSipProvider& operator=(const ExternalSipProvider&) = delete;
};

class SipBridge : public b2bua::Application, public CliHandler {
public:
	SipBridge() = default;

	SipBridge(linphone::Core&, config::v2::Root&&);

	void init(const std::shared_ptr<linphone::Core>& core, const flexisip::GenericStruct& config) override;
	ActionToTake onCallCreate(const linphone::Call& incomingCall, linphone::CallParams& outgoingCallParams) override;
	void onCallEnd(const linphone::Call& call) override;

	std::string handleCommand(const std::string& command, const std::vector<std::string>& args) override;

private:
	std::vector<ExternalSipProvider> providers;
	std::unordered_map<std::string, Account*> occupiedSlots;

	void initFromDescs(linphone::Core&, config::v2::Root&&);
};

} // namespace flexisip::b2bua::bridge
