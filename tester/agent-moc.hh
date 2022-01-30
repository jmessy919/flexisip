/*
    Flexisip, a flexible SIP proxy server with media capabilities.
    Copyright (C) 2010-2022 Belledonne Communications SARL, All rights reserved.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <string>

#include "flexisip/sofia-wrapper/su-root.hh"

#include "flexisip/agent-interface.hh"

namespace flexisip {

class AgentMOC : public Agent {
public:
	const std::shared_ptr<sofiasip::SuRoot>& getRoot() const noexcept override {return mRoot;}
	nta_agent_t* getSofiaAgent() const override {throw std::logic_error{sNotImplementedErr};}

	const char* getServerString() const override {throw std::logic_error{sNotImplementedErr};}
	const std::string& getUniqueId() const override {throw std::logic_error{sNotImplementedErr};}

	std::pair<std::string, std::string> getPreferredIp(const std::string &destination) const override {throw std::logic_error{sNotImplementedErr};}
	std::string getPreferredRoute() const override {throw std::logic_error{sNotImplementedErr};}
	const url_t* getPreferredRouteUrl() const override {throw std::logic_error{sNotImplementedErr};}

	const std::string& getRtpBindIp(bool ipv6 = false) const override {throw std::logic_error{sNotImplementedErr};}
	const std::string& getPublicIp(bool ipv6 = false) const override {throw std::logic_error{sNotImplementedErr};}
	const std::string& getResolvedPublicIp(bool ipv6 = false) const override {throw std::logic_error{sNotImplementedErr};}

	const url_t* getNodeUri() const override {throw std::logic_error{sNotImplementedErr};}
	const url_t* getClusterUri() const override {throw std::logic_error{sNotImplementedErr};}
	const url_t* getDefaultUri() const override {throw std::logic_error{sNotImplementedErr};}

	tport_t* getInternalTport() const override {throw std::logic_error{sNotImplementedErr};}
	DomainRegistrationManager* getDRM() override {throw std::logic_error{sNotImplementedErr};}

	bool isUs(const url_t* url, bool check_aliases = true) const override {throw std::logic_error{sNotImplementedErr};}
	sip_via_t* getNextVia(sip_t* response) override {throw std::logic_error{sNotImplementedErr};}
	int countUsInVia(sip_via_t* via) const override {throw std::logic_error{sNotImplementedErr};}

	url_t* urlFromTportName(su_home_t* home, const tp_name_t* name)  override {throw std::logic_error{sNotImplementedErr};}
	void applyProxyToProxyTransportSettings(tport_t* tp)  override {throw std::logic_error{sNotImplementedErr};}

	std::shared_ptr<Module> findModule(const std::string& moduleName) const  override {throw std::logic_error{sNotImplementedErr};}
	std::shared_ptr<Module> findModuleByFunction(const std::string& moduleFunction) const  override {throw std::logic_error{sNotImplementedErr};}

	su_timer_t* createTimer(int milliseconds, timerCallback cb, void *data, bool repeating=true) override {throw std::logic_error{sNotImplementedErr};}
	void stopTimer(su_timer_t* t) override {throw std::logic_error{sNotImplementedErr};}

	void logEvent(const std::shared_ptr<SipEvent>& ev) override {throw std::logic_error{sNotImplementedErr};}
	void incrReplyStat(int status)  override {throw std::logic_error{sNotImplementedErr};}

	void sendRequestEvent(std::shared_ptr<RequestSipEvent> ev) override {throw std::logic_error{sNotImplementedErr};}
	void sendResponseEvent(std::shared_ptr<ResponseSipEvent> ev) override {throw std::logic_error{sNotImplementedErr};}
	void injectRequestEvent(std::shared_ptr<RequestSipEvent> ev) override {throw std::logic_error{sNotImplementedErr};}
	void injectResponseEvent(std::shared_ptr<ResponseSipEvent> ev) override {throw std::logic_error{sNotImplementedErr};}
	void idle() override {throw std::logic_error{sNotImplementedErr};}

	void send(const std::shared_ptr<MsgSip>& msg, url_string_t const* u, tag_type_t tag, tag_value_t value,
	          ...) override {throw std::logic_error{sNotImplementedErr};}
	void reply(const std::shared_ptr<MsgSip>& msg, int status, char const* phrase, tag_type_t tag,
	           tag_value_t value, ...) override {throw std::logic_error{sNotImplementedErr};}
	Agent* getAgent() override {throw std::logic_error{sNotImplementedErr};}

private:
	std::shared_ptr<sofiasip::SuRoot> mRoot{std::make_shared<sofiasip::SuRoot>()};

	static constexpr auto sNotImplementedErr = "not implemented";
};

}; // namespace flexisip
