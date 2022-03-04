/*
    Flexisip, a flexible SIP proxy server with media capabilities.
    Copyright (C) 2010-2020  Belledonne Communications SARL, All rights reserved.

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

#include <memory>

#include "flexisip/configmanager.hh"
#include "linphone++/linphone.hh"

#include "service-server.hh"
#include <regex>

namespace flexisip {
namespace b2bua {
class IModule {
public:
	virtual void init(const std::shared_ptr<linphone::Core>& core, const flexisip::GenericStruct& config) = 0;
	/**
	 * lets the module run some business logic before the outgoing call is placed.
	 *
	 * @param[out]	outgoingCallParams	the params of the outgoing call to be created. They will be modified according
	 *to the business logic of the module.
	 * @param[out]	incomingCall	the call that triggered the B2BUA.
	 * @return		a reason to abort the bridging and decline the incoming call. Reason::None if the call should go
	 *through.
	 **/
	virtual linphone::Reason onCallCreate(linphone::CallParams& outgoingCallParams,
	                                      const linphone::Call& incomingCall) = 0;
	virtual ~IModule() = default;
};
} // namespace b2bua
class B2buaServer : public ServiceServer,
                    public std::enable_shared_from_this<B2buaServer>,
                    public linphone::CoreListener {
public:
	B2buaServer(const std::shared_ptr<sofiasip::SuRoot>& root);
	~B2buaServer();
	static constexpr const char* confKey = "b2bua::confData";

	void onCallStateChanged(const std::shared_ptr<linphone::Core>& core,
	                        const std::shared_ptr<linphone::Call>& call,
	                        linphone::Call::State state,
	                        const std::string& message) override;

protected:
	void _init() override;
	void _run() override;
	void _stop() override;

private:
	class Init {
	public:
		Init();
	};

	static Init sStaticInit;
	std::shared_ptr<linphone::Core> mCore;
	std::unique_ptr<b2bua::IModule> mModule;
};

} // namespace flexisip
