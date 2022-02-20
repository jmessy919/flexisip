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

#include "b2bua-server.hh"
#include "flexisip/logmanager.hh"
#include "flexisip/utils/sip-uri.hh"
#include "flexisip/configmanager.hh"
#include <mediastreamer2/ms_srtp.h>

using namespace std;
using namespace linphone;



namespace flexisip {

// b2bua namespace to declare internal structures
namespace b2bua {
	struct encryptionConfiguration {
		linphone::MediaEncryption mode;
		std::regex pattern; /**< regular expression applied on the callee sip address, when matched, the associated mediaEncryption mode is used on the output call */
		std::string stringPattern; /**< a string version of the pattern for log purpose as the std::regex does not carry it*/
		encryptionConfiguration(linphone::MediaEncryption p_mode, std::string p_pattern): mode(p_mode), pattern(p_pattern), stringPattern(p_pattern) {};
	};
	struct srtpConfiguration {
		std::list<linphone::SrtpSuite> suites;
		std::regex pattern; /**< regular expression applied on the callee sip address, when matched, the associated SRTP suites are used */
		std::string stringPattern;/**< a string version of the pattern for log purposes as the std::regex does not carry it */
		srtpConfiguration(std::list<linphone::SrtpSuite> p_suites, std::string p_pattern): suites(p_suites), pattern(p_pattern), stringPattern(p_pattern) {};
	};

	struct callsRefs {
		std::shared_ptr<linphone::Call> legA; /**< legA is the incoming call intercepted by the b2bua */
		std::shared_ptr<linphone::Call> legB; /**< legB is the call initiated by the b2bua to the original recipient */
		std::shared_ptr<linphone::Conference> conf; /**< the conference created to connect legA and legB */
	};
}

// unamed namespace for local functions
namespace {
	/**
	 * convert a configuration string to a linphone::MediaEncryption
	 *
	 * @param[in]	configString	the configuration string, one of: zrtp, sdes, dtls-srtp, none
	 * @param[out]	encryptionMode	the converted value, None if the input string was invalid
	 * @return		true if the given string is valid, false otherwise
	 **/
	bool string2MediaEncryption(const std::string configString, linphone::MediaEncryption &encryptionMode) {
		if (configString == std::string{"zrtp"}) { encryptionMode = linphone::MediaEncryption::ZRTP; return true;}
		if (configString == std::string{"sdes"}) { encryptionMode = linphone::MediaEncryption::SRTP; return true;}
		if (configString == std::string{"dtls-srtp"}) { encryptionMode = linphone::MediaEncryption::DTLS; return true;}
		if (configString == std::string{"none"}) { encryptionMode = linphone::MediaEncryption::None; return true;}
		encryptionMode = linphone::MediaEncryption::None;
		return false;
	}

	/**
	 * convert a linphone::MediaEncryption to string
	 *
	 * @param[in]	encryptionMode	The MediaEncryption to be converted
	 * @return	The corresponding string, one of: zrtp, sdes, dtls-srtp, none. An error message if no match were found.
	 **/
	std::string MediaEncryption2string(const linphone::MediaEncryption mode) {
		switch (mode) {
			case linphone::MediaEncryption::ZRTP: return "zrtp";
			case linphone::MediaEncryption::SRTP: return "sdes";
			case linphone::MediaEncryption::DTLS: return "dtls-srtp";
			case linphone::MediaEncryption::None: return "none";
		}
		return "Error - MediaEncryption2string is missing a case of MediaEncryption value";
	}

	/**
	 * convert a configuration string to a linphone::SrtpSuite
	 *
	 * @param[in]	configString	the configuration string, one of: AES_CM_128_HMAC_SHA1_80, AES_CM_128_HMAC_SHA1_32,
	 *								AES_192_CM_HMAC_SHA1_80, AES_192_CM_HMAC_SHA1_32,
	 *								AES_256_CM_HMAC_SHA1_80, AES_256_CM_HMAC_SHA1_32
	 *								AEAD_AES_128_GCM, AEAD_AES_256_GCM
	 * @param[out]	encryptionMode	the converted value, Invalid if the input string was invalid
	 * @return		true if the given string is valid, false otherwise:%s
	 **/
	linphone::SrtpSuite string2SrtpSuite(const std::string configString) {
		if (configString == std::string{"AES_CM_128_HMAC_SHA1_80"}) { return linphone::SrtpSuite::AESCM128HMACSHA180;}
		if (configString == std::string{"AES_CM_128_HMAC_SHA1_32"}) { return linphone::SrtpSuite::AESCM128HMACSHA132;}
		if (configString == std::string{"AES_192_CM_HMAC_SHA1_80"}) { return linphone::SrtpSuite::AES192CMHMACSHA180;}
		if (configString == std::string{"AES_192_CM_HMAC_SHA1_32"}) { return linphone::SrtpSuite::AES192CMHMACSHA132;}
		if (configString == std::string{"AES_256_CM_HMAC_SHA1_80"}) { return linphone::SrtpSuite::AES256CMHMACSHA180;}
		if (configString == std::string{"AES_256_CM_HMAC_SHA1_32"}) { return linphone::SrtpSuite::AES256CMHMACSHA132;}
		if (configString == std::string{"AEAD_AES_128_GCM"}) { return linphone::SrtpSuite::AEADAES128GCM;}
		if (configString == std::string{"AEAD_AES_256_GCM"}) { return linphone::SrtpSuite::AEADAES256GCM;}
		return linphone::SrtpSuite::Invalid;
	}

	std::string SrtpSuite2string(const linphone::SrtpSuite suite) {
		switch (suite) {
			case linphone::SrtpSuite::AESCM128HMACSHA180: return "AES_CM_128_HMAC_SHA1_80";
			case linphone::SrtpSuite::AESCM128HMACSHA132: return "AES_CM_128_HMAC_SHA1_32";
			case linphone::SrtpSuite::AES192CMHMACSHA180: return "AES_192_CM_HMAC_SHA1_80";
			case linphone::SrtpSuite::AES192CMHMACSHA132: return "AES_192_CM_HMAC_SHA1_32";
			case linphone::SrtpSuite::AES256CMHMACSHA180: return "AES_256_CM_HMAC_SHA1_80";
			case linphone::SrtpSuite::AES256CMHMACSHA132: return "AES_256_CM_HMAC_SHA1_32";
			case linphone::SrtpSuite::AEADAES128GCM: return "AEAD_AES_128_GCM";
			case linphone::SrtpSuite::AEADAES256GCM: return "AEAD_AES_256_GCM";
			case linphone::SrtpSuite::Invalid: return "Invalid";
		}
		return "Invalid";
	}
	std::string SrtpSuite2string(const std::list<linphone::SrtpSuite> suites) {
		std::string ret{};
		for(const auto suite:suites) {
			ret.append(SrtpSuite2string(suite) + ", ");
		}
		return ret;
	}
	/**
	 * Explode a string into a vector of strings according to a delimiter
	 *
	 * @param[in]	s 			the string to explode
	 * @param[in]	delimiter	the delimiter to use
	 * @return	a vector of strings
	 */
	std::vector<std::string> explode(const std::string& s, char delimiter)
	{
		std::vector<std::string> tokens;
		std::string token;
		std::istringstream tokenStream(s);
		while (std::getline(tokenStream, token, delimiter))
		{
			tokens.push_back(token);
		}
		return tokens;
	}

	/**
	 * Given one leg of the tranfered call, it returns the other legA
	 *
	 * @param[in]	call one of the call in the two call conference created by the b2bua
	 *
	 * @return	the other call in the conference
	 */
	std::shared_ptr<linphone::Call> getPeerCall(std::shared_ptr<linphone::Call> call) {
		auto &confData = call->getData<flexisip::b2bua::callsRefs>(B2buaServer::confKey);
		if (call->getDir() == linphone::Call::Dir::Outgoing) {
			return confData.legA;
		} else { // This is legA, pause legB
			return confData.legB;
		}
	}
}

B2buaServer::Init B2buaServer::sStaticInit; // The Init object is instanciated to load the config

B2buaServer::B2buaServer (const std::shared_ptr<sofiasip::SuRoot>& root) : ServiceServer(root) {}

B2buaServer::~B2buaServer () {}

void B2buaServer::onConferenceStateChanged(const std::shared_ptr<linphone::Core> & core, const std::shared_ptr<linphone::Conference> & conference,
			linphone::Conference::State state){
	SLOGD<<"b2bua server onConferenceStateChanged to "<<(int)state;
	switch (state) {
		case linphone::Conference::State::None:
			break;
		case linphone::Conference::State::Instantiated:
			break;
		case linphone::Conference::State::CreationPending:
			break;
		case linphone::Conference::State::Created:
			break;
		case linphone::Conference::State::CreationFailed:
			break;
		case linphone::Conference::State::TerminationPending:
			break;
		case linphone::Conference::State::Terminated:
			break;
		case linphone::Conference::State::TerminationFailed:
			break;
		case linphone::Conference::State::Deleted:
			break;
		default:
			break;
    }
}

void B2buaServer::onCallStateChanged(const std::shared_ptr<linphone::Core > &core, const std::shared_ptr<linphone::Call> &call,
			linphone::Call::State state, const std::string &message) {
	SLOGD<<"b2bua server onCallStateChanged to "<<(int)state<<" "<<((call->getDir() == linphone::Call::Dir::Outgoing)?"legB":"legA");
	switch (state) {
		case linphone::Call::State::IncomingReceived:
			{
			auto calleeAddress = call->getToAddress()->asString();
			auto calleeAddressUriOnly = call->getToAddress()->asStringUriOnly();
			auto callerAddress = call->getRemoteAddress()->asString();
			SLOGD<<"b2bua server onCallStateChanged incomingReceived, to "<<calleeAddress<<" from "<<callerAddress;
			// Create outgoing call using parameters created from the incoming call in order to avoid duplicating the callId
			auto outgoingCallParams = mCore->createCallParams(call);
			// add this custom header so this call will not be intercepted by the b2bua
			outgoingCallParams->addCustomHeader("flexisip-b2bua", "ignore");
			outgoingCallParams->setFromHeader(callerAddress);

			// select an outgoing encryption
			bool outgoingEncryptionSet = false;
			for (auto &outEncSetting: mOutgoingEncryption) {
				if (std::regex_match(calleeAddressUriOnly, outEncSetting.pattern)) {
					SLOGD<<"b2bua server: call to "<<calleeAddressUriOnly<<" matches regex "<<outEncSetting.stringPattern<<" assign encryption mode "<<MediaEncryption2string(outEncSetting.mode);
					outgoingCallParams->setMediaEncryption(outEncSetting.mode);
					outgoingEncryptionSet = true;
					break; // stop at the first matching regexp
				}
			}
			if (outgoingEncryptionSet == false) {
				SLOGD<<"b2bua server: call to "<<calleeAddressUriOnly<<" uses incoming encryption setting";
			}

			// When outgoing encryption mode is sdes, select a crypto suite list setting if a pattern matches
			if (outgoingCallParams->getMediaEncryption() == linphone::MediaEncryption::SRTP) {
				for (auto &outSrtpSetting: mSrtpConf) {
					if (std::regex_match(calleeAddressUriOnly, outSrtpSetting.pattern)) {
						SLOGD<<"b2bua server: call to "<<calleeAddressUriOnly<<" matches SRTP suite regex "<<outSrtpSetting.stringPattern<<" assign Srtp Suites to "<<SrtpSuite2string(outSrtpSetting.suites);
						outgoingCallParams->setSrtpSuites(outSrtpSetting.suites);
						break; // stop at the first matching regexp
					}
				}
			}

			// Check the selected outgoing encryption setting is available
			if (!mCore->isMediaEncryptionSupported(outgoingCallParams->getMediaEncryption())) {
				SLOGD<<"b2bua server tries to place an output call using "<<MediaEncryption2string(outgoingCallParams->getMediaEncryption())<<" encryption mode but it is not available";
				call->decline(linphone::Reason::NotAcceptable);
				return;
			}

			// create a conference and attach it
			auto conferenceParams = mCore->createConferenceParams(nullptr);
			conferenceParams->enableVideo(true);
			conferenceParams->enableLocalParticipant(false); // b2bua core is not part of it
			conferenceParams->enableOneParticipantConference(true);
			conferenceParams->setConferenceFactoryAddress(nullptr);

			auto conference = mCore->createConferenceWithParams(conferenceParams);
			conference->addListener(shared_from_this());


			// create legB and add it to the conference
			auto callee = call->getToAddress()->clone();
			auto legB = mCore->inviteAddressWithParams(callee, outgoingCallParams);

			conference->addParticipant(legB);

			// add legA to the conference, but do not answer now
			conference->addParticipant(call);

			// store shared pointer to the conference and each call
			auto confData = new b2bua::callsRefs();
			confData->conf=conference;
			confData->legA = call;
			confData->legB = legB;

			// store ref on each other call
			call->setData<b2bua::callsRefs>(B2buaServer::confKey, *confData);
			legB->setData<b2bua::callsRefs>(B2buaServer::confKey, *confData);
			SLOGD<<"B2bua: End of Incoming call received, conf data is "<< confData;
			}
			break;
		case linphone::Call::State::PushIncomingReceived:
			break;
		case linphone::Call::State::OutgoingInit:
			break;
		case linphone::Call::State::OutgoingProgress:
			break;
		case linphone::Call::State::OutgoingRinging:
		{
			// This is legB getting its ring from callee, relay it to the legA call
			auto &confData = call ->getData<b2bua::callsRefs>(B2buaServer::confKey);
			SLOGD<<"b2bua server onCallStateChanged OutGoingRinging from legB";
			confData.legA->notifyRinging();
		}
			break;
		case linphone::Call::State::OutgoingEarlyMedia:
		{
			// LegB call sends early media: relay a 180
			auto &confData = call->getData<b2bua::callsRefs>(B2buaServer::confKey);
			SLOGD<<"b2bua server onCallStateChanged OutGoing Early media from legB";
			confData.legA->notifyRinging();
		}
			break;
		case linphone::Call::State::Connected:
		{
			// If legB is in connected state, answer legA call
			if (call->getDir() == linphone::Call::Dir::Outgoing) {
				SLOGD<<"b2bua server onCallStateChanged Connected: leg B -> answer legA";
				auto &confData = call->getData<b2bua::callsRefs>(B2buaServer::confKey);
				auto incomingCallParams = mCore->createCallParams(confData.legA);
				// add this custom header so this call will not be intercepted by the b2bua
				incomingCallParams->addCustomHeader("flexisip-b2bua", "ignore");
				confData.legA->acceptWithParams(incomingCallParams);
			}
		}
			break;
		case linphone::Call::State::StreamsRunning:
		{
			auto peerCall = getPeerCall(call);
			// if we are in StreamsRunning but peer is sendonly or inactive we likely arrived here after resuming from pausedByRemote
			// update peer back to recvsend
			auto peerCallAudioDirection = peerCall->getCurrentParams()->getAudioDirection();
			if ( peerCallAudioDirection == linphone::MediaDirection::SendOnly
				|| peerCallAudioDirection == linphone::MediaDirection::Inactive ) {
				SLOGD<<"b2bua server onCallStateChanged: peer call is paused, update it to resume";
				auto peerCallParams = peerCall->getCurrentParams()->copy();
				peerCallParams->setAudioDirection(linphone::MediaDirection::SendRecv);
				peerCall->update(peerCallParams);
			}
		}
			break;
		case linphone::Call::State::Pausing:
			break;
		case linphone::Call::State::Paused:
			break;
		case linphone::Call::State::Resuming:
			break;
		case linphone::Call::State::Referred:
			break;
		case linphone::Call::State::Error:
			// when call in error we shall kill the conf, just do as in End
		case linphone::Call::State::End:
		{
			SLOGD<<"B2bua end call";
			// If there are some data in that call, it is the first one to end
			if (call->dataExists(B2buaServer::confKey)) {
				auto peerCall = getPeerCall(call);

				SLOGD<<"B2bua end call: There is a confData in that ending call";
				auto &confData = call->getData<b2bua::callsRefs>(B2buaServer::confKey);
				// unset data everywhere it was stored
				confData.legA->unsetData(B2buaServer::confKey);
				confData.legB->unsetData(B2buaServer::confKey);
				confData.conf->unsetData(B2buaServer::confKey);
				// terminate peer Call, copy error info from this call
				peerCall->terminateWithErrorInfo(call->getErrorInfo());
				// terminate the conf
				confData.conf->terminate();
				// memory cleaning
				delete(&confData);
			} else {
				SLOGD<<"B2bua end call: There is NO confData in that ending call";
			}
		}
			break;
		case linphone::Call::State::PausedByRemote:
		{
			// Paused by remote: do not pause peer call as it will kick it out of the conference
			// just switch the media direction to sendOnly (only if it is not already set this way)
			auto peerCall = getPeerCall(call);
			auto peerCallParams = peerCall->getCurrentParams()->copy();
			auto audioDirection = peerCallParams->getAudioDirection();
			// Nothing to do if peer call is already not sending audio
			if (audioDirection != linphone::MediaDirection::Inactive && audioDirection != linphone::MediaDirection::SendOnly) {
				peerCallParams->setAudioDirection(linphone::MediaDirection::SendOnly);
				peerCall->update(peerCallParams);
			}
		}
			break;
		case linphone::Call::State::UpdatedByRemote:
		{
			// Manage add/remove video - ignore for other changes
			auto peerCall = getPeerCall(call);
			auto peerCallParams = peerCall->getCurrentParams()->copy();
			auto selfCallParams = call->getCurrentParams()->copy();
			BCTBX_SLOGD<<"UpdatedByRemote -> Self audio "<<selfCallParams->audioEnabled()<<" - Direction "<<(int)selfCallParams->getAudioDirection()<<std::endl
			<<"Peer audio "<<peerCallParams->videoEnabled()<<" - Direction "<<(int)peerCallParams->getVideoDirection();
			bool update=false;
			if (peerCallParams->videoEnabled() != selfCallParams->videoEnabled()) {
				update=true;
				peerCallParams->enableVideo(selfCallParams->videoEnabled());
			}
			if (peerCallParams->audioEnabled() != selfCallParams->audioEnabled()) {
				update=true;
				peerCallParams->enableAudio(selfCallParams->audioEnabled());
			}
			if (update) {
				peerCall->update(peerCallParams);
			}
		}
			break;
		case linphone::Call::State::IncomingEarlyMedia:
			break;
		case linphone::Call::State::Updating:
			break;
		case linphone::Call::State::Released:
			break;
		case linphone::Call::State::EarlyUpdating:
			break;
		case linphone::Call::State::EarlyUpdatedByRemote:
			break;
		default:
			break;
	}
}

void B2buaServer::_init () {
	// Parse configuration for Data Dir
	/* Handle the case where the  directory is not created.
	 * This is for convenience, because our rpm and deb packages create it already. - NO THEY DO NOT DO THAT
	 * However, in other case (like developper environnement) this is painful to create it all the time manually.*/
	auto config = GenericManager::get()->getRoot()->get<GenericStruct>("b2bua-server");
	auto dataDirPath = config->get<ConfigString>("data-directory")->read();
	if (!bctbx_directory_exists(dataDirPath.c_str())) {
		BCTBX_SLOGI<<"Creating b2bua data directory "<<dataDirPath;
		// check parent dir exists as default path requires creation of 2 levels
		auto parentDir = dataDirPath.substr(0, dataDirPath.find_last_of('/'));
		if (!bctbx_directory_exists(parentDir.c_str())) {
			if (bctbx_mkdir(parentDir.c_str()) != 0){
				BCTBX_SLOGE<<"Could not create b2bua data parent directory "<<parentDir;
			}
		}
		if (bctbx_mkdir(dataDirPath.c_str()) != 0){
			BCTBX_SLOGE<<"Could not create b2bua data directory "<<dataDirPath;
		}
	}
	BCTBX_SLOGI<<"B2bua data directory set to "<<dataDirPath;
	Factory::get()->setDataDir(dataDirPath + "/");

	auto configLinphone = Factory::get()->createConfig("");
	configLinphone->setBool("misc", "conference_server_enabled", 1);
	configLinphone->setInt("misc", "max_calls", 1000);
	configLinphone->setInt("misc", "media_resources_mode", 1); // share media resources
	configLinphone->setBool("sip", "reject_duplicated_calls", false);
	configLinphone->setInt("misc", "conference_layout", static_cast<int>(linphone::ConferenceLayout::Legacy));
	mCore = Factory::get()->createCoreWithConfig(configLinphone, nullptr);
	mCore->getConfig()->setString("storage", "backend", "sqlite3");
	mCore->getConfig()->setString("storage", "uri", ":memory:");
	mCore->setUseFiles(true); //No sound card shall be used in calls
	mCore->enableEchoCancellation(false);
	mCore->setPrimaryContact("sip:b2bua@localhost"); //TODO: get the primary contact from config, do we really need one?
	mCore->enableAutoSendRinging(false); // Do not auto answer a 180 on incoming calls, relay the one from the other part.
	mCore->setZrtpSecretsFile("null");

	// random port for UDP audio and video stream
	mCore->setAudioPort(-1);
	mCore->setVideoPort(-1);

	shared_ptr<Transports> b2buaTransport = Factory::get()->createTransports();
	// Get transport from flexisip configuration
	std::string mTransport = config->get<ConfigString>("transport")->read();
	if (mTransport.length() > 0) {
		sofiasip::Home mHome;
		url_t *urlTransport = url_make(mHome.home(), mTransport.c_str());
		if (urlTransport == nullptr || mTransport.at(0) == '<') {
			LOGF("B2bua server: Your configured conference transport(\"%s\") is not an URI.\n"
				"If you have \"<>\" in your transport, remove them.", mTransport.c_str());
		}
		b2buaTransport->setTcpPort(stoi(urlTransport->url_port));
	}

	mCore->setTransports(b2buaTransport);
	mCore->addListener(shared_from_this());


	// create a non registered account to force route outgoing call through the proxy

	auto route = Factory::get()->createAddress(config->get<ConfigString>("outbound-proxy")->read());
	auto accountParams = mCore->createAccountParams();
	accountParams->setIdentityAddress(Factory::get()->createAddress(mCore->getPrimaryContact()));
	accountParams->enableRegister(false);
	accountParams->setServerAddress(route);
	accountParams->setRoutesAddresses({route});
	auto account = mCore->createAccount(accountParams); // store the account pointer in local var to capture it in lambda
	mCore->addAccount(account);
	mCore->setDefaultAccount(account);


	// Parse configuration for outgoing encryption mode
	auto outgoingEncryptionList = config->get<ConfigStringList>("outgoing-enc-regex")->read();
	// parse from the list begining, we shall have couple : encryption_mode regex
	while (outgoingEncryptionList.size()>=2) {
		linphone::MediaEncryption outgoingEncryption = linphone::MediaEncryption::None;
		if (string2MediaEncryption(outgoingEncryptionList.front(), outgoingEncryption)) {
			outgoingEncryptionList.pop_front();
			try {
				mOutgoingEncryption.emplace_back(outgoingEncryption, outgoingEncryptionList.front());
			} catch (exception &e) {
				BCTBX_SLOGE<<"b2bua configuration error: outgoing-enc-regex contains invalid regex : "<<outgoingEncryptionList.front();
			}
			outgoingEncryptionList.pop_front();
		} else {
			BCTBX_SLOGE<<"b2bua configuration error: outgoing-enc-regex contains invalid encryption mode: "<<outgoingEncryptionList.front()<<" valids modes are : zrtp, sdes, dtls-srtp, none. Ignore this setting";
			outgoingEncryptionList.pop_front();
			outgoingEncryptionList.pop_front();
		}
	}

	// Parse configuration for outgoing SRTP suite
	// we shall have a space separated list of suites regex suites regex ... suites regex
	// If no regexp match, use the default configuration from rcfile
	// each suites is a ; separated list of suites
	auto outgoingSrptSuiteList = config->get<ConfigStringList>("outgoing-srtp-regex")->read();
	while (outgoingSrptSuiteList.size()>=2) {
		// first part is a ; separated list of suite, explode it and get each one of them
		auto srtpSuites = explode(outgoingSrptSuiteList.front(), ';');
		std::list<linphone::SrtpSuite> srtpCryptoSuites{};
		// turn the string list into a std::list of linphone::SrtpSuite
		for (auto &suiteName: srtpSuites) {
			srtpCryptoSuites.push_back(string2SrtpSuite(suiteName));
		}
		if (srtpCryptoSuites.size()>0) {
			outgoingSrptSuiteList.pop_front();
			// get the associated regex
			try {
				mSrtpConf.emplace_back(srtpCryptoSuites, outgoingSrptSuiteList.front());
			} catch (exception &e) {
				BCTBX_SLOGE<<"b2bua configuration error: outgoing-srtp-regex contains invalid regex : "<<outgoingSrptSuiteList.front();
			}
			outgoingSrptSuiteList.pop_front();
		} else {
				BCTBX_SLOGE<<"b2bua configuration error: outgoing-srtp-regex contains invalid suite: "<<outgoingSrptSuiteList.front()<<". Ignore this setting";
				outgoingSrptSuiteList.pop_front();
				outgoingSrptSuiteList.pop_front();
		}
	}

	mCore->start();
}

void B2buaServer::_run () {
	mCore->iterate();
}

void B2buaServer::_stop () {
	mCore->removeListener(shared_from_this());
}

B2buaServer::Init::Init() {
	ConfigItemDescriptor items[] = {
		{
			String,
			"transport",
			"SIP uri on which the back-to-back user agent server is listening on.",
			"sip:127.0.0.1:6067;transport=tcp"
		},
		{
			StringList,
			"outgoing-enc-regex",
			"Select the call outgoing encryption mode, this is a list of regular expressions and encryption mode.\n"
			"Valid encryption modes are: zrtp, dtls-srtp, sdes, none.\n\n"
			"The list is formatted in the following mode:\n"
			"mode1 regex1 mode2 regex2 ... moden regexn\n"
			"regex use posix syntax, any invalid one is skipped\n"
			"Each regex is applied, in the given order, on the callee sip uri(including parameters if any). First match found determines the encryption mode. "
			"if no regex matches, the incoming call encryption mode is used.\n\n"
			"Example: zrtp .*@sip\\.secure-example\\.org dtsl-srtp .*dtls@sip\\.example\\.org zrtp .*zrtp@sip\\.example\\.org sdes .*@sip\\.example\\.org\n"
			"In this example: the address is matched in order with\n"
			".*@sip\\.secure-example\\.org so any call directed to an address on domain sip.secure-example-org uses zrtp encryption mode\n"
			".*dtls@sip\\.example\\.org any call on sip.example.org to a username ending with dtls uses dtls-srtp encryption mode\n"
			".*zrtp@sip\\.example\\.org any call on sip.example.org to a username ending with zrtp uses zrtp encryption mode\n"
			"The previous example will fail to match if the call is directed to a specific device(having a GRUU as callee address)\n"
			"To ignore sip URI parameters, use (;.*)? at the end of the regex. Example: .*@sip\\.secure-example\\.org(;.*)?\n"
			"Default:"
			"Selected encryption mode(if any) is enforced and the call will fail if the callee does not support this mode",
			""
		},
		{
			StringList,
			"outgoing-srtp-regex",
			"Outgoing SRTP crypto suite in SDES encryption mode:\n"
			"Select the call outgoing SRTP crypto suite when outgoing encryption mode is SDES, this is a list of regular expressions and crypto suites list.\n"
			"Valid srtp crypto suites are :\n"
			"AES_CM_128_HMAC_SHA1_80, AES_CM_128_HMAC_SHA1_32\n"
			"AES_192_CM_HMAC_SHA1_80, AES_192_CM_HMAC_SHA1_32 // currently not supported\n"
			"AES_256_CM_HMAC_SHA1_80, AES_256_CM_HMAC_SHA1_80\n"
			"AEAD_AES_128_GCM, AEAD_AES_256_GCM // currently not supported\n"
			"\n"
			"The list is formatted in the following mode:\n"
			"cryptoSuiteList1 regex1 cryptoSuiteList2 regex2 ... crytoSuiteListn regexn\n"
			"with cryptoSuiteList being a ; separated list of crypto suites.\n"
			"\n"
			"Regex use posix syntax, any invalid one is skipped\n"
			"Each regex is applied, in the given order, on the callee sip uri(including parameters if any). First match found determines the crypto suite list used.\n"
			"\n"
			"if no regex matches, core setting is applied\n"
			"or default to AES_CM_128_HMAC_SHA1_80;AES_CM_128_HMAC_SHA1_32;AES_256_CM_HMAC_SHA1_80;AES_256_CM_HMAC_SHA1_32 when no core setting is available\n"
			"\n"
			"Example:\n"
			"AES_256_CM_HMAC_SHA1_80;AES_256_CM_HMAC_SHA1_32 .*@sip\\.secure-example\\.org AES_CM_128_HMAC_SHA1_80 .*@sip\\.example\\.org\n"
			"\n"
			"In this example: the address is matched in order with\n"
			".*@sip\\.secure-example\\.org so any call directed to an address on domain sip.secure-example-org uses AES_256_CM_HMAC_SHA1_80;AES_256_CM_HMAC_SHA1_32 suites (in that order)\n"
			".*@sip\\.example\\.org any call directed to an address on domain sip.example.org use AES_CM_128_HMAC_SHA1_80 suite\n"
			"The previous example will fail to match if the call is directed to a specific device(having a GRUU as callee address)\n"
			"To ignore sip URI parameters, use (;.*)? at the end of the regex. Example: .*@sip\\.secure-example\\.org(;.*)?\n"
			"Default:",
			""
		},
		{
			String,
			"data-directory",
			"Directory where to store b2bua core local files\n"
			"Default",
			DEFAULT_B2BUA_DATA_DIR
		},
		{
			String,
			"outbound-proxy",
			"The Flexisip proxy URI to which the B2bua server should send all its outgoing SIP requests.",
			"sip:127.0.0.1:5060;transport=tcp"
		},
		config_item_end
	};

	auto uS = make_unique<GenericStruct>(
	    "b2bua-server",
	    "Flexisip back-to-back user agent server parameters.",
	    0);
	auto s = GenericManager::get()->getRoot()->addChild(move(uS));
	s->addChildrenValues(items);
}

}
