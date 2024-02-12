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

#include "linphone++/linphone.hh"

#include "b2bua/b2bua-server.hh"
#include "b2bua/sip-bridge/accounts/account.hh"
#include "b2bua/sip-bridge/accounts/selection-strategy/account-selection-strategy.hh"
#include "b2bua/sip-bridge/configuration/v2/v2.hh"
#include "b2bua/sip-bridge/invite-tweaker.hh"
#include "b2bua/sip-bridge/trigger-strategy.hh"
#include "cli.hh"

namespace flexisip::b2bua::bridge {

class SipProvider {
	friend class SipBridge;

public:
	// Move constructor
	SipProvider(SipProvider&& other) = default;

	std::optional<b2bua::Application::ActionToTake>
	onCallCreate(const linphone::Call& incomingCall,
	             linphone::CallParams& outgoingCallParams,
	             std::unordered_map<std::string, std::weak_ptr<Account>>& occupiedSlots);
	
	const account_strat::AccountSelectionStrategy& getAccountSelectionStrategy() const;

private:
	SipProvider(std::unique_ptr<trigger_strat::TriggerStrategy>&& triggerStrat,
	            std::unique_ptr<account_strat::AccountSelectionStrategy>&& accountStrat,
	            config::v2::OnAccountNotFound onAccountNotFound,
	            InviteTweaker&& inviteTweaker,
	            std::string&& name);

	std::unique_ptr<trigger_strat::TriggerStrategy> mTriggerStrat;
	std::unique_ptr<account_strat::AccountSelectionStrategy> mAccountStrat;
	config::v2::OnAccountNotFound mOnAccountNotFound;
	InviteTweaker mInviteTweaker;
	std::string name;

	// Disable copy semantics
	SipProvider(const SipProvider&) = delete;
	SipProvider& operator=(const SipProvider&) = delete;
};

using AccountPoolImplMap = std::unordered_map<config::v2::AccountPoolName, std::shared_ptr<AccountPool>>;
class SipBridge : public b2bua::Application, public CliHandler {
public:
	explicit SipBridge(const std::shared_ptr<sofiasip::SuRoot>& suRoot) : mSuRoot{suRoot} {};

	SipBridge(const std::shared_ptr<sofiasip::SuRoot>& suRoot, linphone::Core& core, config::v2::Root&& rootConf);

	void init(const std::shared_ptr<linphone::Core>& core, const flexisip::GenericStruct& config) override;
	ActionToTake onCallCreate(const linphone::Call& incomingCall, linphone::CallParams& outgoingCallParams) override;
	void onCallEnd(const linphone::Call& call) override;

	std::string handleCommand(const std::string& command, const std::vector<std::string>& args) override;

	const std::vector<SipProvider>& getProviders() const {
		return providers;
	}

private:
	AccountPoolImplMap getAccountPoolsFromConfig(linphone::Core& core,
	                                             config::v2::AccountPoolConfigMap& accountPoolConfigMap);
	void initFromRootConfig(linphone::Core& core, config::v2::Root rootConfig);

	std::shared_ptr<sofiasip::SuRoot> mSuRoot;
	std::vector<SipProvider> providers;
	std::unordered_map<std::string, std::weak_ptr<Account>> occupiedSlots;
};

} // namespace flexisip::b2bua::bridge
