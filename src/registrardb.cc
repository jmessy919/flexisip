/*
	Flexisip, a flexible SIP proxy server with media capabilities.
	Copyright (C) 2010-2015  Belledonne Communications SARL, All rights reserved.

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

#include "registrardb.hh"
#include "registrardb-internal.hh"
#ifdef ENABLE_REDIS
#include "registrardb-redis.hh"
#endif
#include "common.hh"

#include "configmanager.hh"

#include <sstream>
#include <ostream>
#include <ctime>
#include <cstdio>
#include <algorithm>
#include <iomanip>

#include <sofia-sip/sip_protos.h>
#include "recordserializer.hh"
#include "module.hh"

using namespace std;

ostream &ExtendedContact::print(std::ostream &stream, time_t now, time_t offset) const {
	char buffer[256] = "UNDETERMINED";
	time_t expire = mExpireAt;
	expire += offset;
	struct tm *ptm = localtime(&expire);
	if (ptm != NULL) {
		strftime(buffer, sizeof(buffer) - 1, "%c", ptm);
	}
	int expireAfter = mExpireAt - now;

	stream << mSipUri << " path=\"";
	for (auto it = mPath.cbegin(); it != mPath.cend(); ++it) {
		if (it != mPath.cbegin())
			stream << " ";
		stream << *it;
	}
	stream << "\"";
	stream << " alias=" << (mAlias ? "yes" : "no");
	if (!mAlias)
		stream << " uid=" << mUniqueId;
	stream << " expire=" << expireAfter << " s (" << buffer << ")";
	return stream;
}

sip_contact_t *ExtendedContact::toSofiaContact(su_home_t *home, time_t now) const {
	sip_contact_t *contact = NULL;
	time_t expire = mExpireAt - now;
	if (expire <= 0)
		return NULL;

	ostringstream oss;
	oss << "<" << mSipUri << ">";
	oss << ";expires=" << expire;
	if (mQ == 0.f) {
		oss.setf(ios::fixed, ios::floatfield);
		oss << std::setprecision(2) << std::setw(4);
		oss << ";q=" << mQ;
	}
	contact = sip_contact_make(home, oss.str().c_str());
	return contact;
}

sip_route_t *ExtendedContact::toSofiaRoute(su_home_t *home) const {
	sip_route_t *rbegin = NULL;
	sip_route_t *r = NULL;
	for (auto it = mPath.begin(); it != mPath.end(); ++it) {
		sip_route_t *newr = sip_route_format(home, "<%s>", (*it).c_str());
		if (!newr) {
			LOGE("Cannot parse %s into route header", (*it).c_str());
			break;
		}
		if (!url_has_param(newr->r_url, "lr")) {
			url_param_add(home, newr->r_url, "lr");
		}
		if (rbegin == NULL) {
			rbegin = newr;
		} else {
			r->r_next = newr;
		}
		r = newr;
	}
	return rbegin;
}

const sip_contact_t *Record::getContacts(su_home_t *home, time_t now) {
	sip_contact_t *alist = NULL;
	for (auto it = mContacts.begin(); it != mContacts.end(); ++it) {
		sip_contact_t *current = (*it)->toSofiaContact(home, now);
		if (current && alist) {
			current->m_next = alist;
		}
		alist = current;
	}
	return alist;
}

bool Record::isInvalidRegister(const std::string &call_id, uint32_t cseq) {
	for (auto it = mContacts.begin(); it != mContacts.end(); ++it) {
		shared_ptr<ExtendedContact> ec = (*it);
		if ((0 == strcmp(ec->callId(), call_id.c_str())) && cseq <= ec->mCSeq) {
			LOGD("CallID %s already registered with CSeq %d (received %d)", call_id.c_str(), ec->mCSeq, cseq);
			return true;
		}
	}
	return false;
}

string Record::extractUniqueId(const sip_contact_t *contact) {
	char lineValue[256] = {0};

	/*search for device unique parameter among the ones configured */
	for (auto it = sLineFieldNames.begin(); it != sLineFieldNames.end(); ++it) {
		const char *ct_param = msg_params_find(contact->m_params, it->c_str());
		if (ct_param)
			return ct_param;
		if (url_param(contact->m_url->url_params, it->c_str(), lineValue, sizeof(lineValue) - 1) > 0) {
			return lineValue;
		}
	}

	return "";
}

const shared_ptr<ExtendedContact> Record::extractContactByUniqueId(std::string uid) {
	const auto contacts = getExtendedContacts();
	for (auto it = contacts.begin(); it != contacts.end(); ++it) {
		const shared_ptr<ExtendedContact> ec = *it;
		if (ec && ec->mUniqueId.compare(uid) == 0) {
			return ec;
		}
	}
	shared_ptr<ExtendedContact> noContact;
	return noContact;
}

/**
 * Should first have checked the validity of the register with isValidRegister.
 */
void Record::clean(const sip_contact_t *sip, const std::string &call_id, uint32_t cseq, time_t now, int version) {
	if (mContacts.begin() == mContacts.end()) {
		return;
	}
	const char *lineValuePtr = NULL;
	string lineValue = extractUniqueId(sip);

	if (!lineValue.empty())
		lineValuePtr = lineValue.c_str();

	auto it = mContacts.begin();
	while (it != mContacts.end()) {
		shared_ptr<ExtendedContact> ec = (*it);
		if (now >= ec->mExpireAt) {
			SLOGD << "Cleaning expired contact " << ec->mContactId;
			it = mContacts.erase(it);
		} else if (ec->line() && lineValuePtr != NULL && 0 == strcmp(ec->line(), lineValuePtr)) {
			SLOGD << "Cleaning older line '" << lineValuePtr << "' for contact " << ec->mContactId;
			it = mContacts.erase(it);
		} else if (0 == strcmp(ec->callId(), call_id.c_str())) {
			SLOGD << "Cleaning same call id contact " << ec->contactId() << "(" << call_id << ")";
			it = mContacts.erase(it);
		} else {
			++it;
		}
	}

	SLOGD << *this;
}

/*
 static int countContacts(const sip_contact_t *contacts) {
 sip_contact_t *c=(sip_contact_t *)contacts;
 int count=0;
 for(;c;c=c->m_next) {
 ++count;
 }

 return count;
 }


 static bool ec_sort(ExtendedContact *ec1, ExtendedContact *ec2) {
 return ec1->mUpdatedTime < ec2->mUpdatedTime;
 }

 void Record::makeSpace(int slots) {
 int spaceNeeded=countContacts(contacts);

 int spaceLeft=sMaxContacts - mContacts.size() - count;
 if (spaceLeft >=0) return;

 LOGD("Need to make space for received contacts (need %i slots)", -spaceLeft);
 sort (mContacts.begin(), mContacts.end(), ec_sort);
 print();

 list<ExtendedContact *>::iterator it;
 for (it=mContacts.begin(); spaceLeft == 0; ++it) {
 it=mContacts.erase(it);
 ++spaceLeft;
 }
 print();
 }
 */

/**
 * Should first have checked the validity of the register with isValidRegister.
 */
void Record::clean(time_t now) {
	auto it = mContacts.begin();
	while (it != mContacts.end()) {
		shared_ptr<ExtendedContact> ec = (*it);
		if (now >= ec->mExpireAt) {
			it = mContacts.erase(it);
		} else {
			++it;
		}
	}
}

time_t Record::latestExpire() const {
	time_t latest = 0;
	for (auto it = mContacts.begin(); it != mContacts.end(); ++it) {
		if ((*it)->mExpireAt > latest)
			latest = (*it)->mExpireAt;
	}
	return latest;
}

time_t Record::latestExpire(Agent *ag) const {
	SofiaAutoHome home;
	time_t latest = 0;
	for (auto it = mContacts.begin(); it != mContacts.end(); ++it) {
		if ((*it)->mPath.empty() || (*it)->mExpireAt <= latest)
			continue;
		std::string paths = *(*it)->mPath.begin();
		url_t *url = url_format(home.home(), "%s", paths.c_str());
		if (ag->isUs(url))
			latest = (*it)->mExpireAt;
	}
	return latest;
}

std::list<std::string> Record::route_to_stl(su_home_t *home, const sip_route_s *route) {
	std::list<std::string> res;
	while (route != NULL) {
		res.push_back(url_as_string(home, route->r_url));
		route = route->r_next;
	}
	return res;
}

string Record::defineKeyFromUrl(const url_t *url) {
	ostringstream ostr;
	if (url->url_user) {
		if (!RegistrarDb::get()->useGlobalDomain()) {
			ostr<<url->url_user<<"@"<<url->url_host;
		} else {
			ostr<<url->url_user<<"@"<<"merged";
		}
	} else {
		ostr<<url->url_host;
	}
	return ostr.str();
}


void Record::insertOrUpdateBinding(const shared_ptr<ExtendedContact> &ec) {
	// Try to locate an existing contact
	shared_ptr<ExtendedContact> olderEc;

	if (sAssumeUniqueDomains && mIsDomain){
		mContacts.clear();
	}

	for (auto it = mContacts.begin(); it != mContacts.end(); ++it) {
		if (0 == strcmp(ec->contactId(), (*it)->contactId())) {
			LOGD("Removing older contact with same id %s", (*it)->contactId());
			mContacts.erase(it);
			mContacts.push_back(ec);
			return;
		}
		if (!olderEc || olderEc->mUpdatedTime > (*it)->mUpdatedTime) {
			olderEc = (*it);
		}
	}

	// If contact doesn't exist and there is space left
	if (mContacts.size() < (unsigned int)sMaxContacts) {
		mContacts.push_back(ec);
	} else { // no space
		mContacts.remove(olderEc);
		mContacts.push_back(ec);
	}
}

static void defineContactId(ostringstream &oss, const url_t *url, const char *transport) {
	if (transport != NULL)
		oss << transport << ":";
	if (url->url_user != NULL)
		oss << url->url_user << ":";
	oss << url->url_host;
	if (url->url_port)
		oss << ":" << url->url_port;
}

void Record::update(const sip_contact_t *contacts, const sip_path_t *path, int globalExpire, const std::string &call_id,
					uint32_t cseq, time_t now, bool alias, const std::list<std::string> accept, bool usedAsRoute) {
	sip_contact_t *c = (sip_contact_t *)contacts;
	list<string> stlPath;

	if (path != NULL) {
		su_home_t home;
		su_home_init(&home);
		stlPath = route_to_stl(&home, path);
		su_home_destroy(&home);
	}

	while (c) {
		if ((c->m_expires && atoi(c->m_expires) == 0) || (!c->m_expires && globalExpire <= 0)) {
			c = c->m_next;
			continue;
		}

		const char *lineValuePtr = NULL;
		string lineValue = extractUniqueId(c);
		if (!lineValue.empty())
			lineValuePtr = lineValue.c_str();

		char transport[20];
		char *transportPtr = transport;
		if (!url_param(c->m_url[0].url_params, "transport", transport, sizeof(transport) - 1)) {
			transportPtr = NULL;
		}

		ostringstream contactId;
		defineContactId(contactId, c->m_url, transportPtr);
		ExtendedContactCommon ecc(contactId.str().c_str(), stlPath, call_id, lineValuePtr);
		auto exc = make_shared<ExtendedContact>(ecc, c, globalExpire, cseq, now, alias, accept);
		exc->mUsedAsRoute = usedAsRoute;
		insertOrUpdateBinding(exc);
		c = c->m_next;
	}

	SLOGD << *this;
}

void Record::update(const ExtendedContactCommon &ecc, const char *sipuri, long expireAt, float q, uint32_t cseq,
					time_t updated_time, bool alias, const std::list<std::string> accept, bool usedAsRoute) {
	auto exct = make_shared<ExtendedContact>(ecc, sipuri, expireAt, q, cseq, updated_time, alias, accept);
	exct->mUsedAsRoute = usedAsRoute;
	insertOrUpdateBinding(exct);
}

void Record::print(std::ostream &stream) const {
	stream << "Record contains " << mContacts.size() << " contacts";
	time_t now = getCurrentTime();
	time_t offset = getTimeOffset(now);

	for (auto it = mContacts.begin(); it != mContacts.end(); ++it) {
		stream << "\n";
		(*it)->print(stream, now, offset);
	}
	stream << "\n==========================";
}

int Record::sMaxContacts = -1;
list<string> Record::sLineFieldNames;
bool Record::sAssumeUniqueDomains = false;

Record::Record(const url_t *aor) : mKey(aor ? defineKeyFromUrl(aor) : "") {
	if (sMaxContacts == -1)
		init();
	if (aor) mIsDomain = aor->url_user == NULL;
}

Record::~Record() {
}

void Record::init() {
	GenericStruct *registrar = GenericManager::get()->getRoot()->get<GenericStruct>("module::Registrar");
	sMaxContacts = registrar->get<ConfigInt>("max-contacts-by-aor")->read();
	sLineFieldNames = registrar->get<ConfigStringList>("unique-id-parameters")->read();
	sAssumeUniqueDomains = GenericManager::get()
								   ->getRoot()
								   ->get<GenericStruct>("inter-domain-connections")
								   ->get<ConfigBoolean>("assume-unique-domains")
								   ->read();
}

void Record::appendContactsFrom(Record *src) {
	if (!src)
		return;

	for (auto it = src->mContacts.begin(); it != src->mContacts.end(); ++it) {
		mContacts.push_back(*it);
	}
}

RegistrarDb::LocalRegExpire::LocalRegExpire(string preferredRoute) {
	mPreferedRoute = preferredRoute;
}

RegistrarDb::RegistrarDb(const string &preferredRoute)
	: mLocalRegExpire(new LocalRegExpire(preferredRoute)), mUseGlobalDomain(false) {
}

RegistrarDb::~RegistrarDb() {
	delete mLocalRegExpire;
}

void RegistrarDb::subscribe(const std::string &topic, const std::shared_ptr<ContactRegisteredListener> &listener) {
	LOGD("Subscribe topic = %s", topic.c_str());
	mContactListenersMap.insert(make_pair(topic, listener));
}

void RegistrarDb::unsubscribe(const std::string &topic) {
	LOGD("Unsubscribe topic = %s", topic.c_str());
	auto it = mContactListenersMap.find(topic);
	if (it != mContactListenersMap.end()) {
		mContactListenersMap.erase(topic);
	}
}

void RegistrarDb::notifyContactListener(const std::string &key, const std::string &uid) {
	LOGD("Notify topic = %s, uid = %s", key.c_str(), uid.c_str());
	auto it = mContactListenersMap.find(key);
	if (it != mContactListenersMap.end()) {
		std::shared_ptr<ContactRegisteredListener> listener = (*it).second;
		listener->onContactRegistered(key, uid);
	}
}

void RegistrarDb::LocalRegExpire::update(const Record &record) {
	unique_lock<mutex> lock(mMutex);
	time_t latest = record.latestExpire(sUnique->mAgent);
	if (latest > 0) {
		auto it = mRegMap.find(record.getKey());
		if (it != mRegMap.end()) {
			(*it).second = latest;
		} else {
			mRegMap.insert(make_pair(record.getKey(), latest));
		}
	} else {
		mRegMap.erase(record.getKey());
	}
}

size_t RegistrarDb::LocalRegExpire::countActives() {
	return mRegMap.size();
}
void RegistrarDb::LocalRegExpire::removeExpiredBefore(time_t before) {
	unique_lock<mutex> lock(mMutex);

	for (auto it = mRegMap.begin(); it != mRegMap.end();) {
		// LOGE("> %s [%lu]", (*it).first.c_str(), (*it).second-before);
		if ((*it).second <= before) {
			auto prevIt = it;
			++it;
			mRegMap.erase(prevIt);
		} else {
			++it;
		}
	}
}

int RegistrarDb::count_sip_contacts(const sip_contact_t *contact) {
	int count = 0;
	sip_contact_t *current = (sip_contact_t *)contact;
	while (current) {
		if (!current->m_expires || atoi(current->m_expires) != 0){
			++count;
		}
		current = current->m_next;
	}
	return count;
}

bool RegistrarDb::errorOnTooMuchContactInBind(const sip_contact_t *sip_contact, const string & key,
											  const shared_ptr<RegistrarDbListener> &listener) {
	int nb_contact = count_sip_contacts(sip_contact);
	int max_contact = Record::getMaxContacts();
	if (nb_contact > max_contact) {
		LOGD("Too many contacts in register %s %i > %i", key.c_str(), nb_contact,
			 max_contact);
		return true;
	}

	return false;
}

RegistrarDb *RegistrarDb::sUnique = NULL;

RegistrarDb *RegistrarDb::initialize(Agent *ag){
	if (sUnique != NULL){
		LOGF("RegistrarDb already initialized");
	}
	GenericStruct *cr = GenericManager::get()->getRoot();
	GenericStruct *mr = cr->get<GenericStruct>("module::Registrar");
	GenericStruct *mro = cr->get<GenericStruct>("module::Router");

	bool useGlobalDomain = mro->get<ConfigBoolean>("use-global-domain")->read();
	string dbImplementation = mr->get<ConfigString>("db-implementation")->read();
	if ("internal" == dbImplementation) {
		LOGI("RegistrarDB implementation is internal");
		sUnique = new RegistrarDbInternal(ag->getPreferredRoute());
		sUnique->mUseGlobalDomain = useGlobalDomain;
	}
#ifdef ENABLE_REDIS
	/* Previous implementations allowed "redis-sync" and "redis-async", whereas we now expect "redis".
		* We check that the dbImplementation _starts_ with "redis" now, so that we stay backward compatible. */
	else if (dbImplementation.find("redis") == 0) {
		LOGI("RegistrarDB implementation is REDIS");
		GenericStruct *registrar = GenericManager::get()->getRoot()->get<GenericStruct>("module::Registrar");
		RedisParameters params;
		params.domain = registrar->get<ConfigString>("redis-server-domain")->read();
		params.port = registrar->get<ConfigInt>("redis-server-port")->read();
		params.timeout = registrar->get<ConfigInt>("redis-server-timeout")->read();
		params.auth = registrar->get<ConfigString>("redis-auth-password")->read();
		params.mSlaveCheckTimeout = registrar->get<ConfigInt>("redis-slave-check-period")->read();

		sUnique = new RegistrarDbRedisAsync(ag, params);
		sUnique->mUseGlobalDomain = useGlobalDomain;
	}
#endif
	else {
		LOGF("Unsupported implementation '%s'. %s",
#ifdef ENABLE_REDIS
				"Supported implementations are 'internal' or 'redis'.", dbImplementation.c_str());
#else
				"Supported implementation is 'internal'.", dbImplementation.c_str());
#endif
	}
	sUnique->mAgent = ag;
	return sUnique;
}

RegistrarDb *RegistrarDb::get() {
	if (sUnique == NULL) {
		LOGF("RegistrarDb not initialized.");
	}
	return sUnique;
}

void RegistrarDb::clear(const sip_t *sip, const shared_ptr<RegistrarDbListener> &listener) {
	doClear(sip, listener);
}

void RegistrarDb::clear(const url_t *url,
                        const std::string& callId,
                        const std::shared_ptr<RegistrarDbListener>& listener) {
	// Forged message
	MsgSip msg(nta_msg_create(mAgent->getSofiaAgent(), 0));
	auto home = msg.getHome();
	auto sip = msg.getSip();
	sip->sip_from = sip_from_create(home, reinterpret_cast<const url_string_t*>(url));
	sip->sip_call_id = sip_call_id_make(home, callId.c_str());
	sip->sip_cseq = sip_cseq_create(home, 0xDEADC0DE, SIP_METHOD_REGISTER); // Placeholder
	clear(sip, listener);
}

class RecursiveRegistrarDbListener : public RegistrarDbListener,
									 public enable_shared_from_this<RecursiveRegistrarDbListener> {
  private:
	RegistrarDb *m_database;
	shared_ptr<RegistrarDbListener> mOriginalListener;
	Record *m_record;
	su_home_t m_home;
	int m_request;
	int m_step;
	const char *m_url;
	static int sMaxStep;

  public:
	RecursiveRegistrarDbListener(RegistrarDb *database, const shared_ptr<RegistrarDbListener> &original_listerner,
								 const url_t *url, int step = sMaxStep)
		: m_database(database), mOriginalListener(original_listerner), m_request(1), m_step(step) {
		m_record = new Record(url);
		su_home_init(&m_home);
		m_url = url_as_string(&m_home, url);
	}

	~RecursiveRegistrarDbListener() {
		su_home_deinit(&m_home);
		delete m_record;
	}

	void onRecordFound(Record *r) {
		if (r != NULL) {
			auto &extlist = r->getExtendedContacts();
			list<sip_contact_t *> vectToRecurseOn;
			for (auto it = extlist.begin(); it != extlist.end(); ++it) {
				shared_ptr<ExtendedContact> ec = *it;
				// Also add alias for late forking (context in the forks map for this alias key)
				SLOGD << "Step: " << m_step << (ec->mAlias ? "\tFound alias " : "\tFound contact ") << m_url << " -> "
					  << ec->mSipUri << " usedAsRoute:" << ec->mUsedAsRoute;
				if (!ec->mAlias && ec->mUsedAsRoute) {
					ec = transformContactUsedAsRoute(m_url, ec);
				}
				m_record->pushContact(ec);
				if (ec->mAlias && m_step > 0) {
					sip_contact_t *contact = sip_contact_format(&m_home, "<%s>", ec->mSipUri.c_str());
					if (contact) {
						vectToRecurseOn.push_back(contact);
					} else {
						SLOGW << "Can't create sip_contact of " << ec->mSipUri;
					}
				}
			}
			m_request += vectToRecurseOn.size();
			for (auto itrec = vectToRecurseOn.cbegin(); itrec != vectToRecurseOn.cend(); ++itrec) {
				m_database->fetch((*itrec)->m_url,
								  make_shared<RecursiveRegistrarDbListener>(m_database, this->shared_from_this(),
																			(*itrec)->m_url, m_step - 1),
								  false);
			}
		}

		if (waitPullUpOrFail()) {
			SLOGD << "Step: " << m_step << "\tNo contact found for " << m_url;
			mOriginalListener->onRecordFound(NULL);
		}
	}

	void onError() {
		SLOGW << "Step: " << m_step << "\tError during recursive fetch of " << m_url;
		if (waitPullUpOrFail()) {
			mOriginalListener->onError();
		}
	}

	void onInvalid() {
		SLOGW << "Step: " << m_step << "\tInvalid during recursive fetch of " << m_url;
		if (waitPullUpOrFail()) {
			mOriginalListener->onInvalid();
		}
	}

  private:
	shared_ptr<ExtendedContact> transformContactUsedAsRoute(const char *uri, const shared_ptr<ExtendedContact> &ec) {
		/*this function does the following:
		 * - make a copy of the extended contact
		 * - in this copy replace the main contact information by the 'url' given in argument
		 * - append the main contact information of the original extended contact into the Path header of the new
		 * extended contact.
		 * While recursiving through alias, this allows to have a Route header appended for a "usedAsRoute" kind of
		 * Contact but still preserving
		 * the last request uri that was found recursed through the alias mechanism.
		*/
		shared_ptr<ExtendedContact> newEc = make_shared<ExtendedContact>(*ec);
		newEc->mSipUri = uri;
		newEc->mPath.push_back(ec->mSipUri);
		// LOGD("transformContactUsedAsRoute(): path to %s added for %s", ec->mSipUri.c_str(), uri);
		newEc->mUsedAsRoute = false;
		return newEc;
	}

	bool waitPullUpOrFail() {
		if (--m_request != 0)
			return false; // wait for all pending responses

		// No more results expected for this recursion level
		if (m_record->getExtendedContacts().empty()) {
			return true; // no contacts collected on below recursion levels
		}

		// returning records collected on below recursion levels
		SLOGD << "Step: " << m_step << "\tReturning collected records " << m_record->getExtendedContacts().size();
		mOriginalListener->onRecordFound(m_record);
		return false;
	}
};

// Max recursive step
int RecursiveRegistrarDbListener::sMaxStep = 1;

RegistrarDbListener::~RegistrarDbListener() {
}

ContactRegisteredListener::~ContactRegisteredListener() {
}

void RegistrarDb::fetch(const url_t *url, const shared_ptr<RegistrarDbListener> &listener, bool recursive) {
	fetch(url, listener, false, recursive);
}

void RegistrarDb::fetch(const url_t *url, const std::shared_ptr<RegistrarDbListener> &listener, bool includingDomains,
						bool recursive) {
	if (includingDomains) {
		fetchWithDomain(url, listener, recursive);
	} else {
		if (recursive) {
			doFetch(url, make_shared<RecursiveRegistrarDbListener>(this, listener, url));
		} else {
			doFetch(url, listener);
		}
	}
}

class AgregatorRegistrarDbListener : public RegistrarDbListener {
  private:
	shared_ptr<RegistrarDbListener> mOriginalListener;
	int mNumRespExpected;
	int mNumResponseObtained;
	Record *mRecord;
	bool mError;
	Record *getRecord() {
		if (mRecord == NULL)
			mRecord = new Record(NULL);
		return mRecord;
	}
	void checkFinished() {
		mNumResponseObtained++;
		if (mNumResponseObtained == mNumRespExpected) {
			if (mError && mRecord == NULL) {
				mOriginalListener->onError();
			} else {
				mOriginalListener->onRecordFound(mRecord);
			}
		}
	}

  public:
	AgregatorRegistrarDbListener(const shared_ptr<RegistrarDbListener> &origListener, int numResponseExpected)
		: mOriginalListener(origListener), mNumRespExpected(numResponseExpected), mNumResponseObtained(0), mRecord(0) {
		mError = false;
	}
	virtual ~AgregatorRegistrarDbListener() {
		if (mRecord)
			delete mRecord;
	}
	virtual void onRecordFound(Record *r) {
		if (r) {
			getRecord()->appendContactsFrom(r);
		}
		checkFinished();
	}
	virtual void onError() {
		mError = true;
		checkFinished();
	}
	virtual void onInvalid() {
		// onInvalid() will normally never be called for a fetch request
		checkFinished();
	}
};

void RegistrarDb::fetchWithDomain(const url_t *url, const std::shared_ptr<RegistrarDbListener> &listener,
								  bool recursive) {
	url_t domainOnlyUrl = *url;
	domainOnlyUrl.url_user = NULL;

	if (url->url_user) {
		auto agregator = make_shared<AgregatorRegistrarDbListener>(listener, 2);
		fetch(url, agregator, recursive);
		fetch(&domainOnlyUrl, agregator, false);
		return;
	}

	fetch(&domainOnlyUrl, listener, false);
}

RecordSerializer *RecordSerializer::create(const string &name) {
	if (name == "c") {
		return new RecordSerializerC();
	} else if (name == "json") {
		return new RecordSerializerJson();
	}
#if ENABLE_PROTOBUF
	else if (name == "protobuf") {
		return new RecordSerializerPb();
	}
#endif
#if ENABLE_MSGPACK
	else if (name == "msgpack"){
		return new RecordSerializerMsgPack();
	}
#endif
	else {
		return NULL;
	}
}

RecordSerializer *RecordSerializer::sInstance = NULL;

RecordSerializer *RecordSerializer::get() {
	if (!sInstance) {
		GenericStruct *registrar = GenericManager::get()->getRoot()->get<GenericStruct>("module::Registrar");
		string name = registrar->get<ConfigString>("redis-record-serializer")->read();

		sInstance = create(name);
		if (!sInstance) {
			LOGF("Unsupported record serializer: '%s'", name.c_str());
		}
	}

	return sInstance;
}
