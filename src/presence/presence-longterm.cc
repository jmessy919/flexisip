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

#include <belle-sip/belle-sip.h>
#include <belle-sip/sip-uri.h>

#include "flexisip/registrar/registar-listeners.hh"

#include "presence-longterm.hh"
#include "presentity-presenceinformation.hh"
#include "registrar/extended-contact.hh"
#include "registrar/record.hh"
#include "registrar/registrar-db.hh"

using namespace flexisip;
using namespace std;

class PresenceAuthListener : public AuthDbListener {
public:
	PresenceAuthListener(belle_sip_main_loop_t* mainLoop, const std::shared_ptr<PresentityPresenceInformation>& info)
	    : mMainLoop(mainLoop), mInfo(info) {
		AuthDbBackend::get(); /*this will initialize the database backend, which is good to know that it works at
		                         startup*/
	}
	PresenceAuthListener(belle_sip_main_loop_t* mainLoop,
	                     std::map<std::string, std::shared_ptr<PresentityPresenceInformation>>& dInfo)
	    : mMainLoop(mainLoop), mDInfo(dInfo) {
		AuthDbBackend::get(); /*this will initialize the database backend, which is good to know that it works at
		                         startup*/
	}

	void onResult(AuthDbResult result, const std::string& passwd) override {
		auto func = [this, result, passwd]() { processResponse(result, passwd); };
		belle_sip_main_loop_cpp_do_later(mMainLoop, func, "OnAuthListener to mainthread");
	}

	void onResult(AuthDbResult result, const vector<passwd_algo_t>& passwd) override {
		auto func = [this, result, passwd]() { processResponse(result, passwd.front().pass); };
		belle_sip_main_loop_cpp_do_later(mMainLoop, func, "OnAuthListener to mainthread");
	}

private:
	void processResponse(AuthDbResult result, const std::string& user) {
		shared_ptr<PresentityPresenceInformation> info = mInfo ? mInfo : mDInfo.find(user)->second;

		const char* cuser = belle_sip_uri_get_user(info->getMainEntity());
		if (result == AuthDbResult::PASSWORD_FOUND) {
			auto isPhone = false;
			const auto userParam = belle_sip_uri_get_user_param(info->getMainEntity());
			if (userParam) {
				isPhone = strcmp(belle_sip_uri_get_user_param(info->getMainEntity()), "phone") == 0;
			}
			// result is a phone alias if (and only if) user is not the same as the entity user
			auto isAlias = strcmp(user.c_str(), cuser) != 0;
			belle_sip_uri_t* uri = BELLE_SIP_URI(belle_sip_object_clone(BELLE_SIP_OBJECT(info->getMainEntity())));
			char* contactString = belle_sip_uri_to_string(uri);
			if (isAlias || isPhone) {
				// change contact accordingly
				belle_sip_parameters_t* params = BELLE_SIP_PARAMETERS(uri);
				belle_sip_parameters_remove_parameter(params, "user");
				belle_sip_uri_set_user(uri, user.c_str());
				SLOGD << __FILE__ << ": "
				      << "Found user " << user << " for alias/phone " << belle_sip_uri_get_user(info->getMainEntity())
				      << ", adding contact " << uri << " presence information";
				info->setDefaultElement(uri);
			} else {
				SLOGD << __FILE__ << ": "
				      << "Found user " << user << ", adding presence information";
				info->setDefaultElement();
			}
			belle_sip_object_unref(uri);

			class InternalListListener : public ContactUpdateListener {
			public:
				InternalListListener(shared_ptr<PresentityPresenceInformation> info) : mInfo(info) {
				}

				void onRecordFound(const std::shared_ptr<Record>& record) {
					if (!record) return;

					for (const auto& extendedContact : record->getExtendedContacts()) {
						const string specs = extendedContact->getOrgLinphoneSpecs();
						if (!specs.empty()) mInfo->addCapability(specs);
					}
				}
				void onError() {
				}
				void onInvalid() {
				}
				void onContactUpdated(const std::shared_ptr<ExtendedContact>&) {
				}

				su_home_t* getHome() {
					return mHome.home();
				}

			private:
				sofiasip::Home mHome;
				shared_ptr<PresentityPresenceInformation> mInfo;
			};

			// Fetch Redis info.
			shared_ptr<InternalListListener> listener = make_shared<InternalListListener>(info);
			RegistrarDb::get()->fetch(SipUri{contactString}, listener);
			belle_sip_free(contactString);
		} else {
			SLOGD << __FILE__ << ": "
			      << "Could not find user " << cuser << ".";
		}
		delete this;
	}

	belle_sip_main_loop_t* mMainLoop;
	const shared_ptr<PresentityPresenceInformation> mInfo;
	map<string, shared_ptr<PresentityPresenceInformation>> mDInfo;
};

void PresenceLongterm::onListenerEvent(const shared_ptr<PresentityPresenceInformation>& info) const {
	if (!info->hasDefaultElement()) {
		// no presence information know yet, so ask again to the db.
		const belle_sip_uri_t* uri = info->getMainEntity();
		SLOGD << "No presence info element known yet for " << belle_sip_uri_get_user(uri)
		      << ", checking if this user is already registered";
		AuthDbBackend::get().getUserWithPhone(belle_sip_uri_get_user(uri), belle_sip_uri_get_host(uri),
		                                      new PresenceAuthListener(mMainLoop, info));
	}
}
void PresenceLongterm::onListenerEvents(list<shared_ptr<PresentityPresenceInformation>>& infos) const {
	list<tuple<string, string, AuthDbListener*>> creds;
	map<string, shared_ptr<PresentityPresenceInformation>> dInfo;
	for (const shared_ptr<PresentityPresenceInformation>& info : infos) {
		const belle_sip_uri_t* uri = info->getMainEntity();
		if (!info->hasDefaultElement()) {
			creds.push_back(make_tuple(belle_sip_uri_get_user(uri), belle_sip_uri_get_host(uri),
			                           new PresenceAuthListener(mMainLoop, info)));
		}
		dInfo.insert(pair<string, shared_ptr<PresentityPresenceInformation>>(belle_sip_uri_get_user(uri), info));
	}
	AuthDbBackend::get().getUsersWithPhone(creds);
}
