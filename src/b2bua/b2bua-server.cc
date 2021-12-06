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
}

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
		std::list<MSCryptoSuite> suites;
		std::regex pattern; /**< regular expression applied on the callee sip address, when matched, the associated SRTP suites are used */
		std::string stringPattern;/**< a string version of the pattern for log purposes as the std::regex does not carry it */
		srtpConfiguration(std::list<MSCryptoSuite> p_suites, std::string p_pattern): suites(p_suites), pattern(p_pattern), stringPattern(p_pattern) {};
	};

	struct callsRefs {
		std::shared_ptr<linphone::Call> legA;
		std::shared_ptr<linphone::Call> legB;
		std::shared_ptr<linphone::Conference> conf;
	};
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
	SLOGD<<"b2bua server onCallStateChanged to "<<(int)state;
	switch (state) {
		case linphone::Call::State::IncomingReceived:
			{
			auto calleeAddress = call->getToAddress()->asString();
			auto calleeAddressUriOnly = call->getToAddress()->asStringUriOnly();
			auto callerAddress = call->getRemoteAddress()->asString();
			SLOGD<<"b2bua server onCallStateChanged incomingReceived, to "<<calleeAddress<<" from "<<callerAddress;
			// Create outgoing call using parameters created from the incoming call in order to avoid duplicating the callId
			auto outgoingCallParams = mCore->createCallParams(nullptr); //(call);
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
				SLOGD<<"b2bua server: call to "<<calleeAddressUriOnly<<" uses default outgoing encryption setting : "<<MediaEncryption2string(mDefaultOutgoingEncryption);
				outgoingCallParams->setMediaEncryption(mDefaultOutgoingEncryption);
			}

			// create a conference and attach it
			auto conferenceParams = mCore->createConferenceParams();
			conferenceParams->setVideoEnabled(false);
			conferenceParams->setLocalParticipantEnabled(false); // b2bua core is not part of it
			conferenceParams->setOneParticipantConferenceEnabled(true);

			auto conference = mCore->createConferenceWithParams(conferenceParams);
			conference->addListener(shared_from_this());


			// create legB and add it to the conference
			auto callee = call->getToAddress()->clone();
			auto legB = mCore->inviteAddressWithParams(callee, outgoingCallParams);

			conference->addParticipant(legB); // TODO: call SHOULD be added here to the conf to avoid RE-INVITE if added at StreamRunning state

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
			auto &confData = call->getData<b2bua::callsRefs>(B2buaServer::confKey);
			std::shared_ptr<linphone::Call> peerCall = nullptr;
			// get peerCall
			if (call->getDir() == linphone::Call::Dir::Outgoing) {
				SLOGD<<"b2bua server onCallStateChanged: leg B";
				peerCall = confData.legA;
			} else { // This is legA, pause legB
				SLOGD<<"b2bua server onCallStateChanged: leg A";
				peerCall = confData.legB;
			}
			// if we are in StreamsRunning but peer is sendonly we likely arrived here after resuming from pausedByRemote
			// update peer back to recvsend
			if (peerCall->getCurrentParams()->getAudioDirection() == linphone::MediaDirection::SendOnly) {
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
			break;
		case linphone::Call::State::End:
		{
			SLOGD<<"B2bua end call";
			// If there are some data in that call, it is the first one to end
			if (call->dataExists(B2buaServer::confKey)) {
				SLOGD<<"B2bua end call: There is a confData in that ending call";
				auto &confData = call->getData<b2bua::callsRefs>(B2buaServer::confKey);
				// unset data everywhere it wasa stored
				confData.legA->unsetData(B2buaServer::confKey);
				confData.legB->unsetData(B2buaServer::confKey);
				confData.conf->unsetData(B2buaServer::confKey);
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
			// just switch the media direction to sendOnly
			auto &confData = call->getData<b2bua::callsRefs>(B2buaServer::confKey);
			std::shared_ptr<linphone::Call> peerCall = nullptr;
			// Is this the legB call?
			if (call->getDir() == linphone::Call::Dir::Outgoing) {
				SLOGD<<"b2bua server onCallStateChanged PausedByRemote: leg B";
				peerCall = confData.legA;
			} else { // This is legA, pause legB
				SLOGD<<"b2bua server onCallStateChanged PausedByRemote: leg A";
				peerCall = confData.legB;
			}
			auto peerCallParams = peerCall->getCurrentParams()->copy();
			peerCallParams->setAudioDirection(linphone::MediaDirection::SendOnly);
			peerCall->update(peerCallParams);
		}
			break;
		case linphone::Call::State::UpdatedByRemote:
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
	auto configLinphone = Factory::get()->createConfig("");
	configLinphone->setBool("misc", "conference_server_enabled", 1);
	configLinphone->setInt("misc", "max_calls", 1000);
	configLinphone->setInt("misc", "media_resources_mode", 1); // share media resources
	configLinphone->setBool("sip", "reject_duplicated_calls", false);
	mCore = Factory::get()->createCoreWithConfig(configLinphone, nullptr);
	//mCore->setCallLogsDatabasePath(" ");
	//mCore->setZrtpSecretsFile(" ");
	mCore->getConfig()->setString("storage", "backend", "sqlite3");
	mCore->getConfig()->setString("storage", "uri", ":memory:");
	mCore->setUseFiles(true); //No sound card shall be used in calls
	mCore->enableEchoCancellation(false);
	mCore->setPrimaryContact("sip:b2bua@192.168.1.100"); //TODO: get the primary contact from config, do we really need one?
	mCore->enableAutoSendRinging(false); // Do not auto answer a 180 on incoming calls, relay the one from the other part.

	// random port for UDP audio stream
	mCore->setAudioPort(-1);

	shared_ptr<Transports> b2buaTransport = Factory::get()->createTransports();
	// Get transport from flexisip configuration
	auto config = GenericManager::get()->getRoot()->get<GenericStruct>("b2bua-server");
	string mTransport = config->get<ConfigString>("transport")->read();
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

	// Parse configuration for outgoing encryption mode
	auto outgoingEncryptionList = config->get<ConfigStringList>("outgoing-enc-regex")->read();
	// list must be odd sized, the last element is the default encryption mode
	if (outgoingEncryptionList.size()%2 == 1) {
		// get the last element, it should be the default
		if (!string2MediaEncryption(outgoingEncryptionList.back(), mDefaultOutgoingEncryption)) {
			BCTBX_SLOGE<<"b2bua configuration error: outgoing-enc-regex contains invalid default encryption mode: "<<outgoingEncryptionList.back()<<" valids modes are : zrtp, sdes, dtls-srtp, none. Stick to ZRTP as default";
			mDefaultOutgoingEncryption = linphone::MediaEncryption::ZRTP;
		}
		outgoingEncryptionList.pop_back();

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
	} else {
		BCTBX_SLOGE<<"b2bua configuration error: outgoing-enc-regex size is "<<outgoingEncryptionList.size()<<" but it must be odd. Use default ZRTP encryption for all calls";
	}

	// Parse configuration for outgoing SRTP suite
	// we shall have a space separated list of suites regex suites regex ... suites regex
	// each suites is a comma separated list of suites
	auto outgoingSrptSuiteList = config->get<ConfigStringList>("outgoing-srtp-regex")->read();
	while (outgoingSrptSuiteList.size()>=2) {
		// first part is a comma separated list of suite, explode it and get each one of them
		auto srtpSuites = explode(outgoingSrptSuiteList.front(), ',');
		std::list<MSCryptoSuite> srtpCryptoSuites{};
		// turn the string list into a std::list of MSCryptoSuite
		for (auto &suiteName: srtpSuites) {
			MSCryptoSuiteNameParams suiteNameParam;
			suiteNameParam.name=suiteName.c_str();
			suiteNameParam.params=nullptr; // This parsing does not support params on the suite as they would be space separated
			srtpCryptoSuites.push_back(ms_crypto_suite_build_from_name_params(&suiteNameParam));
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
			" Select the call outgoing encryption mode, this is a list of regular expressions and encryption mode."
			"valid encryption modes are: zrtp, dtls-srtp, sdes, none. The list is formatted in the following mode:"
			"mode1 regex1 mode2 regex2 ... defaultMode"
			"Each regex is applied, in the given order, on the callee sip uri. First match found determines the encryption mode"
			"if no regex matches, the modeFinal is applied"
			"Example: zrtp .*@sip.secure-example.org dtsl-srtp .*dtls@sip.example.org zrtp .*zrtp@sip.example.org sdes .*@sip.example.org none"
			"In this example: the address is matched in order with"
			" .*@sip.secure-example.org so any call directed to an address on domain sip.secure-example-org uses zrtp encryption mode"
			" .*dtls@sip.example.org any call on sip.example.org to a username ending with dtls uses dtls-srtp encryption mode"
			" .*zrtp@sip.example.org any call on sip.example.org to a username ending with zrtp uses zrtp encryption mode"
			" .*@sip.example.org if no match were found yet on domain sip.example.org, use sdes encryption mode"
			" other domains default to no encryption (this is not recommended)",
			"zrtp"
		},
		{
			StringList,
			"outgoing-srtp-regex",
			" TODO: add description here",
			""
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
