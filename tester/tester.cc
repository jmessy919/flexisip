/*
 Flexisip, a flexible SIP proxy server with media capabilities.
 Copyright (C) 2010-2021  Belledonne Communications SARL, All rights reserved.

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

#include "tester.hh"
#include "bctoolbox/logging.h"

#ifdef HAVE_CONFIG_H
#include "flexisip-config.h"
#endif

std::string bcTesterFile(const std::string& name) {
	char* file = bc_tester_file(name.c_str());
	std::string ret(file);
	bc_free(file);
	return ret;
}

std::string bcTesterRes(const std::string& name) {
	char* file = bc_tester_res(name.c_str());
	std::string ret(file);
	bc_free(file);
	return ret;
}

static int verbose_arg_func(const char* arg) {
	bctbx_set_log_level(NULL, BCTBX_LOG_DEBUG);
	return 0;
}

int main(int argc, char* argv[]) {
	int ret;

	flexisip_tester_init(NULL);

	for (auto i = 1; i < argc; ++i) {
		ret = bc_tester_parse_args(argc, argv, i);
		if (ret > 0) {
			i += ret - 1;
			continue;
		} else if (ret < 0) {
			bc_tester_helper(argv[0], "");
		}
		return ret;
	}

	ret = bc_tester_start(argv[0]);
	flexisip_tester_uninit();
	return ret;
}

static void log_handler(int lev, const char* fmt, va_list args) {
#ifdef _WIN32
	vfprintf(lev == BCTBX_LOG_ERROR ? stderr : stdout, fmt, args);
	fprintf(lev == BCTBX_LOG_ERROR ? stderr : stdout, "\n");
#else
	va_list cap;
	va_copy(cap, args);
	/* Otherwise, we must use stdio to avoid log formatting (for autocompletion etc.) */
	vfprintf(lev == BCTBX_LOG_ERROR ? stderr : stdout, fmt, cap);
	fprintf(lev == BCTBX_LOG_ERROR ? stderr : stdout, "\n");
	va_end(cap);
#endif
}

void flexisip_tester_init(void (*ftester_printf)(int level, const char* fmt, va_list args)) {
	bc_tester_set_verbose_func(verbose_arg_func);

	if (ftester_printf == nullptr)
		ftester_printf = log_handler;
	bc_tester_init(ftester_printf, BCTBX_LOG_MESSAGE, BCTBX_LOG_ERROR, ".");

	bc_tester_add_suite(&agent_suite);
	bc_tester_add_suite(&boolean_expressions_suite);
#if ENABLE_CONFERENCE
	bc_tester_add_suite(&conference_suite);
#endif
	bc_tester_add_suite(&extended_contact_suite);
	bc_tester_add_suite(&fork_context_suite);
	bc_tester_add_suite(&module_pushnitification_suite);
#if ENABLE_UNIT_TESTS_PUSH_NOTIFICATION
	bc_tester_add_suite(&push_notification_suite);
#endif
	bc_tester_add_suite(&register_suite);
	bc_tester_add_suite(&router_suite);
	bc_tester_add_suite(&tls_connection_suite);
#if ENABLE_B2BUA
	bc_tester_add_suite(&b2bua_suite);
#endif

#ifdef ENABLE_UNIT_TESTS_MYSQL
	bc_tester_add_suite(&fork_context_mysql_suite);
#endif
	/*
	#if ENABLE_CONFERENCE
	    bc_tester_add_suite(&registration_event_suite);
	#endif
	*/
}

void flexisip_tester_uninit(void) {
	bc_tester_uninit();
}

#include "flexisip/registrardb.hh"
/**
 * A class to manage the flexisip proxy server
 */
Server::Server(const std::string& configFile) {
	// Agent initialisation
	mRoot = std::make_shared<sofiasip::SuRoot>();
	mAgent = std::make_shared<flexisip::Agent>(mRoot);

	if (!configFile.empty()) {
		flexisip::GenericManager *cfg = flexisip::GenericManager::get();

		auto configFilePath = bcTesterRes(configFile);
		int ret=-1;
		if (bctbx_file_exist(configFilePath.c_str()) == 0 ) {
			ret = cfg->load(configFilePath);
		} else {
			ret = cfg->load(std::string(TESTER_DATA_DIR).append(configFile));
		}
		if (ret!=0) {
			BC_FAIL("Unable to load configuration file");
		}
		mAgent->loadConfig(cfg);
	}
}
Server::~Server() {
	mAgent->unloadConfig();
	flexisip::RegistrarDb::resetDB();
}

/**
 * Class to manage a client Core
 */
CoreClient::CoreClient(std::string me) {
	mMe = linphone::Factory::get()->createAddress(me);

	mCore = linphone::Factory::get()->createCore("","", nullptr);
	mCore->getConfig()->setString("storage", "backend", "sqlite3");
	mCore->getConfig()->setString("storage", "uri", ":memory:");
	mCore->getConfig()->setString("storage", "call_logs_db_uri", "null");
	mCore->setZrtpSecretsFile("");
	std::shared_ptr<linphone::Transports> clientTransport = linphone::Factory::get()->createTransports();
	clientTransport->setTcpPort(-2); // -2 for LC_SIP_TRANSPORT_DONTBIND)
	mCore->setTransports(clientTransport);
	mCore->setZrtpSecretsFile("null");
	mCore->setAudioPort(-1);
	mCore->setVideoPort(-1);
	mCore->setUseFiles(true);
	mCore->enableVideoCapture(true); // We must be able to simulate capture to make video calls
	mCore->enableVideoDisplay(false); // No need to bother displaying the received video
	// final check on call successfully established is based on bandwidth used,
	// so use file as input to make sure there is some traffic
	auto helloPath = bcTesterRes("sounds/hello8000.wav");
	if (bctbx_file_exist(helloPath.c_str()) != 0) {
		BC_FAIL("Unable to find resource sound, did you forget to use --resource-dir option?");
	} else {
		mCore->setPlayFile(helloPath);
	}
	auto nowebcamPath = bcTesterRes("images/nowebcamCIF.jpg");
	if (bctbx_file_exist(nowebcamPath.c_str()) != 0) {
		BC_FAIL("Unable to find resource sound, did you forget to use --resource-dir option?");
	} else {
		mCore->setStaticPicture(nowebcamPath);
	}
	auto policy = linphone::Factory::get()->createVideoActivationPolicy();
	policy->setAutomaticallyAccept(true);
	policy->setAutomaticallyInitiate(false); // requires explicit settings in the parameters to initiate a video call
	mCore->setVideoActivationPolicy(policy);
	mCore->start();
}

CoreClient::CoreClient(std::string me, std::shared_ptr<Server> server) : CoreClient(me) {
	registerTo(server);
}

void CoreClient::registerTo(std::shared_ptr<Server> server) {
	mServer = server;

	// Clients register to the first of the list of transports read in the proxy configuration
	auto route = linphone::Factory::get()->createAddress(
		flexisip::GenericManager::get()->getRoot()->get<flexisip::GenericStruct>("global")->get<flexisip::ConfigStringList>("transports")->read().front());

	auto clientAccountParams = mCore->createAccountParams();
	clientAccountParams->setIdentityAddress(mMe);
	clientAccountParams->enableRegister(true);
	clientAccountParams->setServerAddress(route);
	clientAccountParams->setRoutesAddresses({route});
	auto account = mCore->createAccount(clientAccountParams); // store the account pointer in local var to capture it in lambda
	mCore->addAccount(account);
	mAccount = account;

	BC_ASSERT_TRUE(CoreAssert({mCore}, server->getAgent()).wait([account] {
		return account->getState() == linphone::RegistrationState::Ok;
	}));
}

CoreClient::~CoreClient() {
	auto core = mCore;
	if (mAccount != nullptr) {
		auto account = mAccount;
		mCore->clearAccounts();
		BC_ASSERT_TRUE(CoreAssert({mCore}, mServer->getAgent()).wait([account] {
			return account->getState() == linphone::RegistrationState::Cleared;
		}));
	}
	mCore->stopAsync(); // stopAsync is not really async, we must clear the account first or it will wait for the unregistration on server
	CoreAssert({mCore}, mServer->getAgent()).wait([core] {
		return core->getGlobalState() == linphone::GlobalState::Off;
	});
}

std::shared_ptr<linphone::Call> CoreClient::callVideo(std::shared_ptr<CoreClient> callee,
											std::shared_ptr<linphone::CallParams> callerCallParams,
											std::shared_ptr<linphone::CallParams> calleeCallParams) {
	std::shared_ptr<linphone::CallParams> callParams = callerCallParams;
	if (callParams == nullptr) {
		callParams = mCore->createCallParams(nullptr);
	}
	callParams->enableVideo(true);
	return call(callee, callParams, calleeCallParams);
};

std::shared_ptr<linphone::Call> CoreClient::call(std::shared_ptr<CoreClient> callee,
											std::shared_ptr<linphone::CallParams> callerCallParams,
											std::shared_ptr<linphone::CallParams> calleeCallParams) {
	std::shared_ptr<linphone::CallParams> callParams = callerCallParams;
	if (callParams == nullptr) {
		callParams = mCore->createCallParams(nullptr);
	}
	auto callerCall = mCore->inviteAddressWithParams(callee->getAccount()->getContactAddress(), callParams);

	if (callerCall == nullptr) {
		BC_FAIL("Invite failed");
		return nullptr;
	}

	// Check call get the incoming call and caller is in OutgoingRinging state
	if (!BC_ASSERT_TRUE(CoreAssert({mCore, callee->getCore()}, mServer->getAgent()).wait([callee] {
		return ( (callee->getCore()->getCurrentCall() != nullptr)
		&& (callee->getCore()->getCurrentCall()->getState() == linphone::Call::State::IncomingReceived) );
	}))) {
		return nullptr;
	}
	auto calleeCall = callee->getCore()->getCurrentCall();
	if (calleeCall == nullptr) {
		BC_FAIL("No call received");
		return nullptr;
	}

	if (!BC_ASSERT_TRUE(CoreAssert({mCore, callee->getCore()}, mServer->getAgent()).wait([callerCall] {
		return (callerCall->getState() == linphone::Call::State::OutgoingRinging);
	}))) {
		return nullptr;
	}

	// Callee answer the call
	if (!BC_ASSERT_TRUE(calleeCall->acceptWithParams(calleeCallParams) == 0)) {
		return nullptr;
	};

	if (!BC_ASSERT_TRUE(CoreAssert({mCore, callee->getCore()}, mServer->getAgent()).wait([calleeCall, callerCall] {
		return ( callerCall->getState() == linphone::Call::State::StreamsRunning
			&& calleeCall->getState() == linphone::Call::State::StreamsRunning );
	}))) {
		return nullptr;
	}

	auto timeout = std::chrono::seconds(2);
	// If this is a video call, force Fps to generate traffic
	if (callParams->videoEnabled()) {
		mCore->setStaticPictureFps(24.0);
		calleeCall->getCore()->setStaticPictureFps(24.0);
		// give more time for videocall to fully establish to cover ZRTP case that start video channel after the audio channel is secured
		timeout = std::chrono::seconds(6);
	}

	if (!BC_ASSERT_TRUE(CoreAssert({mCore, callee->getCore()}, mServer->getAgent()).waitUntil(timeout, [calleeCall,callerCall,callParams] {
		// Check both sides for download and upload
		bool ret = (calleeCall->getAudioStats() && callerCall->getAudioStats() && calleeCall->getAudioStats()->getDownloadBandwidth() > 10 && callerCall->getAudioStats()->getDownloadBandwidth() > 10)
			&& (calleeCall->getAudioStats()->getUploadBandwidth() > 10 && callerCall->getAudioStats()->getUploadBandwidth() > 10);
		if (callParams->videoEnabled()) { // check on original callParams given, not current one as callee could have refused the video
			ret = ret && (calleeCall->getVideoStats() && callerCall->getVideoStats() && calleeCall->getVideoStats()->getDownloadBandwidth() > 10 && callerCall->getVideoStats()->getDownloadBandwidth() > 10
				&& calleeCall->getVideoStats()->getUploadBandwidth() > 10 && callerCall->getVideoStats()->getUploadBandwidth());
		}
		return ret;
	}))) {
		return nullptr;
	}

	return callerCall;
}

void CoreClient::endCurrentCall(std::shared_ptr<CoreClient> peer) {
	auto selfCall = mCore->getCurrentCall();
	auto peerCall = peer->getCore()->getCurrentCall();
	if (selfCall == nullptr || peerCall == nullptr) {
		BC_FAIL("Trying to end call but No current call running");
		return;
	}
	mCore->getCurrentCall()->terminate();
	BC_ASSERT_TRUE(CoreAssert({mCore, peer->getCore()}, mServer->getAgent()).wait([selfCall, peerCall] {
		return (selfCall->getState() == linphone::Call::State::Released && peerCall->getState() == linphone::Call::State::Released);
	}));
}
