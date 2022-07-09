/*
    Flexisip, a flexible SIP proxy server with media capabilities.
    Copyright (C) 2012  Belledonne Communications SARL.

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

#include <flexisip/agent.hh>
#include <flexisip/module.hh>
#include <flexisip/registrardb.hh>

#include <sofia-sip/sip_status.h>
#include <sofia-sip/su_random.h>

#include <signal.h>

namespace flexisip {

struct RegistrarStats {
	std::unique_ptr<StatPair> mCountBind;
	std::unique_ptr<StatPair> mCountClear;
	StatCounter64* mCountLocalActives;
};

class ModuleRegistrar;
class ResponseContext;

// Listener class NEED to copy the shared pointer
class OnRequestBindListener : public ContactUpdateListener {
	ModuleRegistrar* mModule{nullptr};
	std::shared_ptr<RequestSipEvent> mEv{};
	sip_from_t* mSipFrom{nullptr};
	su_home_t mHome{};
	sip_contact_t* mContact{nullptr};
	sip_path_t* mPath{nullptr};

public:
	OnRequestBindListener(ModuleRegistrar* module,
	                      std::shared_ptr<RequestSipEvent> ev,
	                      const sip_from_t* sipuri = nullptr,
	                      sip_contact_t* contact = nullptr,
	                      sip_path_t* path = nullptr);
	~OnRequestBindListener();

	void onContactUpdated(const std::shared_ptr<ExtendedContact>& ec) override;
	void onRecordFound(const std::shared_ptr<Record>& r) override;
	void onError() override;
	void onInvalid() override;
};

class OnResponseBindListener : public ContactUpdateListener {
	ModuleRegistrar* mModule;
	std::shared_ptr<ResponseSipEvent> mEv;
	std::shared_ptr<OutgoingTransaction> mTr;
	std::shared_ptr<ResponseContext> mCtx;

public:
	OnResponseBindListener(ModuleRegistrar* module,
	                       std::shared_ptr<ResponseSipEvent> ev,
	                       std::shared_ptr<OutgoingTransaction> tr,
	                       std::shared_ptr<ResponseContext> ctx);
	void onContactUpdated(const std::shared_ptr<ExtendedContact>& ec) override;
	void onRecordFound(const std::shared_ptr<Record>& r) override;
	void onError() override;
	void onInvalid() override;
};

// Listener class NEED to copy the shared pointer
class OnStaticBindListener : public ContactUpdateListener {
	friend class ModuleRegistrar;
	sofiasip::Home mHome;
	std::string mContact;
	std::string mFrom;

public:
	OnStaticBindListener(const url_t* from, const sip_contact_t* ct);

	void onContactUpdated(const std::shared_ptr<ExtendedContact>& ec) override;
	void onRecordFound(const std::shared_ptr<Record>& r) override;
	void onError() override;
	void onInvalid() override;
};

class FakeFetchListener : public ContactUpdateListener {
	friend class ModuleRegistrar;

public:
	FakeFetchListener();
	void onContactUpdated(const std::shared_ptr<ExtendedContact>& ec) override;
	void onRecordFound(const std::shared_ptr<Record>& r) override;
	void onError() override;
	void onInvalid() override;
};

class ResponseContext {
public:
	ResponseContext(const std::shared_ptr<RequestSipEvent>& ev, int globalDelta);
	const std::shared_ptr<RequestSipEvent> mRequestSipEvent;
	sip_contact_t* mOriginalContacts{nullptr};
};

class ModuleRegistrar : public Module, public ModuleToolbox {
	friend class OnRequestBindListener;
	friend class OnResponseBindListener;

public:
	explicit ModuleRegistrar(std::weak_ptr<AgentInternalInterface> ag);

	void onDeclare(GenericStruct* mc) override;
	void onLoad(const GenericStruct* mc) override;
	void onUnload() override;
	void onRequest(std::shared_ptr<RequestSipEvent>& ev) override;
	void onResponse(std::shared_ptr<ResponseSipEvent>& ev) override;

	template <typename SipEventT, typename ListenerT>
	void processUpdateRequest(std::shared_ptr<SipEventT>& ev, const sip_t* sip);

	void idle();

	void
	reply(std::shared_ptr<RequestSipEvent>& ev, int code, const char* reason, const sip_contact_t* contacts = nullptr);

	void readStaticRecords();

private:
	std::shared_ptr<ResponseContext> createResponseContext(const std::shared_ptr<RequestSipEvent>& ev, int globalDelta);
	void deleteResponseContext(const std::shared_ptr<ResponseContext>& ctx);

	static void sighandler(int signum, siginfo_t* info, void* ptr);

	void updateLocalRegExpire();
	bool isManagedDomain(const url_t* url);
	std::string routingKey(const url_t* sipUri);
	void removeInternalParams(sip_contact_t* ct);

	RegistrarStats mStats;
	bool mUpdateOnResponse;
	bool mAllowDomainRegistrations;
	std::list<std::string> mDomains;
	std::list<std::string> mUniqueIdParams;
	std::string mServiceRoute;
	static std::list<std::string> mPushNotifParams;
	std::string mRoutingParam;
	unsigned int mMaxExpires, mMinExpires;
	std::string mStaticRecordsFile;
	su_timer_t* mStaticRecordsTimer{nullptr};
	int mStaticRecordsTimeout;
	int mStaticRecordsVersion{0};
	bool mAssumeUniqueDomains;
	struct sigaction mSigaction;
	static ModuleInfo<ModuleRegistrar> sInfo;
	bool mUseGlobalDomain;
	int mExpireRandomizer;
	std::list<std::string> mParamsToRemove;
};

class RegistrarMgt {
public:
	virtual unsigned long long int getTotalNumberOfAddedRecords() = 0;
	virtual unsigned long long int getTotalNumberOfExpiredRecords() = 0;
};

} // namespace flexisip
