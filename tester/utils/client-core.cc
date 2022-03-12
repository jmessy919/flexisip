/*
 Flexisip, a flexible SIP proxy server with media capabilities.
 Copyright (C) 2010-2022  Belledonne Communications SARL, All rights reserved.

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

#include "client-core.hh"
#include "asserts.hh"

/**
 * Class to manage a client Core
 */
CoreClient::CoreClient(const std::string me) {
	mMe = linphone::Factory::get()->createAddress(me);

	mCore = linphone::Factory::get()->createCore("","", nullptr);
	mCore->setPrimaryContact(me);
	mCore->getConfig()->setString("storage", "backend", "sqlite3");
	mCore->getConfig()->setString("storage", "uri", ":memory:");
	mCore->getConfig()->setString("storage", "call_logs_db_uri", "null");
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
		BC_FAIL("Unable to find resource images, did you forget to use --resource-dir option?");
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

}

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

	std::initializer_list<std::shared_ptr<linphone::Core>> coreList={mCore, callee->getCore()};
	// Check call get the incoming call and caller is in OutgoingRinging state
	if (!BC_ASSERT_TRUE(CoreAssert(coreList, mServer->getAgent()).wait([callee] {
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

	if (!BC_ASSERT_TRUE(CoreAssert(coreList, mServer->getAgent()).wait([callerCall] {
		return (callerCall->getState() == linphone::Call::State::OutgoingRinging);
	}))) {
		return nullptr;
	}

	// Callee answer the call
	if (!BC_ASSERT_TRUE(calleeCall->acceptWithParams(calleeCallParams) == 0)) {
		return nullptr;
	};

	if (!BC_ASSERT_TRUE(CoreAssert(coreList, mServer->getAgent()).wait([calleeCall, callerCall] {
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

	if (!BC_ASSERT_TRUE(CoreAssert(coreList, mServer->getAgent()).waitUntil(timeout, [calleeCall,callerCall,callParams] {
		// Check both sides for download and upload
		bool ret = (calleeCall->getAudioStats() && callerCall->getAudioStats() && calleeCall->getAudioStats()->getDownloadBandwidth() > 10 && callerCall->getAudioStats()->getDownloadBandwidth() > 10);
		if (callParams->videoEnabled()) { // check on original callParams given, not current one as callee could have refused the video
			ret = ret && (calleeCall->getVideoStats() && callerCall->getVideoStats() && calleeCall->getVideoStats()->getDownloadBandwidth() > 10 && callerCall->getVideoStats()->getDownloadBandwidth() > 10);
		} else {
			ret = ret && (callerCall->getCurrentParams()->videoEnabled() == false) && (calleeCall->getCurrentParams()->videoEnabled() == false);
		}
		return ret;
	}))) {
		return nullptr;
	}

	return callerCall;
}

bool CoreClient::callUpdate(std::shared_ptr<CoreClient> peer, std::shared_ptr<linphone::CallParams> callParams)  {
	if (callParams == nullptr) {
		BC_FAIL("Cannot update call without new call params");
	}

	auto selfCall = mCore->getCurrentCall();
	auto peerCall = peer->getCore()->getCurrentCall();
	if (selfCall == nullptr || peerCall == nullptr) {
		BC_FAIL("Trying to update a call but at least one participant is not currently engaged in one");
		return false;
	}

	// peer is set to auto accept update so just check the changes after
	selfCall->update(callParams);

	// Wait for the update to be concluded
	if (!BC_ASSERT_TRUE(CoreAssert({mCore, peer->getCore()}, mServer->getAgent()).wait([selfCall, peerCall] {
		return ( selfCall->getState() == linphone::Call::State::StreamsRunning
			&& peerCall->getState() == linphone::Call::State::StreamsRunning );
	}))) return false;

	auto timeout = std::chrono::seconds(2);
	// If this is a video call, force Fps to generate traffic (This must be done once the video call is ongoing)
	if (callParams->videoEnabled()) {
		mCore->setStaticPictureFps(24.0);
		peerCall->getCore()->setStaticPictureFps(24.0);
		// give more time for videocall to fully establish to cover ZRTP case that start video channel after the audio channel is secured
		timeout = std::chrono::seconds(6);
	}

	if (!BC_ASSERT_TRUE(CoreAssert({mCore, peer->getCore()}, mServer->getAgent()).waitUntil(timeout, [peerCall,selfCall,callParams] {
		// Check both sides for download and upload
		bool ret = (peerCall->getAudioStats() && selfCall->getAudioStats() && peerCall->getAudioStats()->getDownloadBandwidth() > 10 && selfCall->getAudioStats()->getDownloadBandwidth() > 10);
		if (callParams->videoEnabled()) { // check on original callParams given, not current one as callee could have refused the video
			ret = ret && (peerCall->getVideoStats() && selfCall->getVideoStats() && peerCall->getVideoStats()->getDownloadBandwidth() > 10 && selfCall->getVideoStats()->getDownloadBandwidth() > 10);
		} else {
			ret = ret && (selfCall->getCurrentParams()->videoEnabled() == false) && (peerCall->getCurrentParams()->videoEnabled() == false);
		}
		return ret;
	}))) return false;

	return true;
}

bool CoreClient::endCurrentCall(std::shared_ptr<CoreClient> peer) {
	auto selfCall = mCore->getCurrentCall();
	auto peerCall = peer->getCore()->getCurrentCall();
	if (selfCall == nullptr || peerCall == nullptr) {
		BC_FAIL("Trying to end call but No current call running");
		return false;
	}
	mCore->getCurrentCall()->terminate();
	if (!BC_ASSERT_TRUE(CoreAssert({mCore, peer->getCore()}, mServer->getAgent()).wait([selfCall, peerCall] {
		return (selfCall->getState() == linphone::Call::State::Released && peerCall->getState() == linphone::Call::State::Released);
	}))) return false;
	return true;
}
