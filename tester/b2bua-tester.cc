/*
 * Copyright (C) 2020 Belledonne Communications SARL
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#include <bctoolbox/logging.h>

#include <linphone++/linphone.hh>

#include "flexisip/agent.hh"
#include "flexisip/configmanager.hh"
#include "flexisip/sofia-wrapper/su-root.hh"

#include "conference/conference-server.hh"
#include "registration-events/client.hh"
#include "registration-events/server.hh"
#include "tester.hh"
#include "b2bua/b2bua-server.hh"

using namespace std;
using namespace linphone;
using namespace flexisip;


namespace b2buatester {
class B2buaServer : public Server {
private:
	std::shared_ptr<flexisip::B2buaServer> mB2buaServer;
public:
	B2buaServer(const std::string& configFile=std::string()) : Server(configFile) {
		// Configure B2bua Server
		GenericStruct *b2buaServerConf = GenericManager::get()->getRoot()->get<GenericStruct>("b2bua-server");
		// b2bua server needs an outbound proxy to route all sip messages to the proxy, set it to the internal-transport of the proxy
		b2buaServerConf->get<ConfigString>("outbound-proxy")->set(GenericManager::get()->getRoot()->get<GenericStruct>("cluster")->get<ConfigString>("internal-transport")->read());
		// need a writable dir to store DTLS-SRTP self signed certificate
		b2buaServerConf->get<ConfigString>("data-directory")->set(bc_tester_get_writable_dir_prefix());

		mB2buaServer = make_shared<flexisip::B2buaServer>(this->getRoot());
		mB2buaServer->init();

		// Configure module b2bua
		GenericManager::get()->getRoot()->get<GenericStruct>("module::B2bua")->get<ConfigString>("b2bua-server")->set(b2buaServerConf->get<ConfigString>("transport")->read());

		// Start proxy
		this->start();
	}
	~B2buaServer() {
		mB2buaServer->stop();
	}
};

// Basic call not using the B2bua server
static void basic() {
	// Create a server and start it
	auto server = std::make_shared<Server>("/config/flexisip_b2bua.conf");
	// flexisip_b2bua config file enables the module B2bua in proxy, disable it for this basic test
	GenericManager::get()->getRoot()->get<GenericStruct>("module::B2bua")->get<ConfigBoolean>("enabled")->set("false");
	server->start();
	{
		// create clients and register them on the server
		// do it in a block to make sure they are destroyed before the server

		// creation and registration in one call
		auto marie = std::make_shared<CoreClient>("sip:marie@sip.example.org", server);
		// creation then registration
		auto pauline = std::make_shared<CoreClient>("sip:pauline@sip.example.org");
		BC_ASSERT_PTR_NULL(pauline->getAccount()); // Pauline account in not available yet, only after registration on the server
		pauline->registerTo(server);
		BC_ASSERT_PTR_NOT_NULL(pauline->getAccount()); // Pauline account in now available

		marie->call(pauline);
		pauline->endCurrentCall(marie);
	}
}


// Scenario: b2b use the same encryption in and out
static void forward() {
	// initialize and start the proxy and B2bua server
	auto server = std::make_shared<B2buaServer>("/config/flexisip_b2bua.conf");
	{
		auto marie = std::make_shared<CoreClient>("sip:marie@sip.example.org", server);
		auto pauline = std::make_shared<CoreClient>("sip:pauline@sip.example.org", server);

		// SDES
		auto callParams = marie->getCore()->createCallParams(nullptr);
		callParams->setMediaEncryption(linphone::MediaEncryption::SRTP);
		marie->call(pauline, callParams);
		BC_ASSERT_TRUE(marie->getCore()->getCurrentCall()->getCurrentParams()->getMediaEncryption() == linphone::MediaEncryption::SRTP);
		BC_ASSERT_TRUE(pauline->getCore()->getCurrentCall()->getCurrentParams()->getMediaEncryption() == linphone::MediaEncryption::SRTP);
		BC_ASSERT_TRUE(marie->getCore()->getCurrentCall()->getCallLog()->getCallId() != pauline->getCore()->getCurrentCall()->getCallLog()->getCallId());
		marie->endCurrentCall(pauline);

		// ZRTP
		callParams = marie->getCore()->createCallParams(nullptr);
		callParams->setMediaEncryption(linphone::MediaEncryption::ZRTP);
		marie->call(pauline, callParams);
		BC_ASSERT_TRUE(marie->getCore()->getCurrentCall()->getCurrentParams()->getMediaEncryption() == linphone::MediaEncryption::ZRTP);
		BC_ASSERT_TRUE(pauline->getCore()->getCurrentCall()->getCurrentParams()->getMediaEncryption() == linphone::MediaEncryption::ZRTP);
		BC_ASSERT_TRUE(marie->getCore()->getCurrentCall()->getCallLog()->getCallId() != pauline->getCore()->getCurrentCall()->getCallLog()->getCallId());
		marie->endCurrentCall(pauline);

		// DTLS
		callParams = marie->getCore()->createCallParams(nullptr);
		callParams->setMediaEncryption(linphone::MediaEncryption::DTLS);
		marie->call(pauline, callParams);
		BC_ASSERT_TRUE(marie->getCore()->getCurrentCall()->getCurrentParams()->getMediaEncryption() == linphone::MediaEncryption::DTLS);
		BC_ASSERT_TRUE(pauline->getCore()->getCurrentCall()->getCurrentParams()->getMediaEncryption() == linphone::MediaEncryption::DTLS);
		BC_ASSERT_TRUE(marie->getCore()->getCurrentCall()->getCallLog()->getCallId() != pauline->getCore()->getCurrentCall()->getCallLog()->getCallId());
		marie->endCurrentCall(pauline);
	}
}

static void sdes2zrtp() {
	// initialize and start the proxy and B2bua server
	auto server = std::make_shared<B2buaServer>("/config/flexisip_b2bua.conf");
	{
		// Create and register clients
		auto sdes = std::make_shared<CoreClient>("sip:b2bua_srtp@sip.example.org", server);
		auto zrtp = std::make_shared<CoreClient>("sip:b2bua_zrtp@sip.example.org", server);

		// Call from SDES to ZRTP
		auto sdesCallParams = sdes->getCore()->createCallParams(nullptr);
		sdesCallParams->setMediaEncryption(linphone::MediaEncryption::SRTP);
		sdes->call(zrtp, sdesCallParams);
		BC_ASSERT_TRUE(sdes->getCore()->getCurrentCall()->getCurrentParams()->getMediaEncryption() == linphone::MediaEncryption::SRTP);
		BC_ASSERT_TRUE(zrtp->getCore()->getCurrentCall()->getCurrentParams()->getMediaEncryption() == linphone::MediaEncryption::ZRTP);
		sdes->endCurrentCall(zrtp);

		// Call from ZRTP to SDES
		auto zrtpCallParams = zrtp->getCore()->createCallParams(nullptr);
		zrtpCallParams->setMediaEncryption(linphone::MediaEncryption::ZRTP);
		zrtp->call(sdes, zrtpCallParams);
		BC_ASSERT_TRUE(sdes->getCore()->getCurrentCall()->getCurrentParams()->getMediaEncryption() == linphone::MediaEncryption::SRTP);
		BC_ASSERT_TRUE(zrtp->getCore()->getCurrentCall()->getCurrentParams()->getMediaEncryption() == linphone::MediaEncryption::ZRTP);
		sdes->endCurrentCall(zrtp);
	}
}

static void sdes2dtls() {
	// initialize and start the proxy and B2bua server
	auto server = std::make_shared<B2buaServer>("/config/flexisip_b2bua.conf");
	{
		// Create and register clients
		auto sdes = std::make_shared<CoreClient>("sip:b2bua_srtp@sip.example.org", server);
		auto dtls = std::make_shared<CoreClient>("sip:b2bua_dtls@sip.example.org", server);

		// Call from SDES to DTLS
		auto sdesCallParams = sdes->getCore()->createCallParams(nullptr);
		sdesCallParams->setMediaEncryption(linphone::MediaEncryption::SRTP);
		sdes->call(dtls, sdesCallParams);
		BC_ASSERT_TRUE(sdes->getCore()->getCurrentCall()->getCurrentParams()->getMediaEncryption() == linphone::MediaEncryption::SRTP);
		BC_ASSERT_TRUE(dtls->getCore()->getCurrentCall()->getCurrentParams()->getMediaEncryption() == linphone::MediaEncryption::DTLS);
		sdes->endCurrentCall(dtls);

		// Call from DTLS to SDES
		auto dtlsCallParams = dtls->getCore()->createCallParams(nullptr);
		dtlsCallParams->setMediaEncryption(linphone::MediaEncryption::DTLS);
		dtls->call(sdes, dtlsCallParams);
		BC_ASSERT_TRUE(sdes->getCore()->getCurrentCall()->getCurrentParams()->getMediaEncryption() == linphone::MediaEncryption::SRTP);
		BC_ASSERT_TRUE(dtls->getCore()->getCurrentCall()->getCurrentParams()->getMediaEncryption() == linphone::MediaEncryption::DTLS);
		sdes->endCurrentCall(dtls);
	}
}

static void zrtp2dtls() {
	// initialize and start the proxy and B2bua server
	auto server = std::make_shared<B2buaServer>("/config/flexisip_b2bua.conf");
	{
		// Create and register clients
		auto zrtp = std::make_shared<CoreClient>("sip:b2bua_zrtp@sip.example.org", server);
		auto dtls = std::make_shared<CoreClient>("sip:b2bua_dtls@sip.example.org", server);

		// Call from ZRTP to DTLS
		auto zrtpCallParams = zrtp->getCore()->createCallParams(nullptr);
		zrtpCallParams->setMediaEncryption(linphone::MediaEncryption::ZRTP);
		zrtp->call(dtls, zrtpCallParams);
		BC_ASSERT_TRUE(zrtp->getCore()->getCurrentCall()->getCurrentParams()->getMediaEncryption() == linphone::MediaEncryption::ZRTP);
		BC_ASSERT_TRUE(dtls->getCore()->getCurrentCall()->getCurrentParams()->getMediaEncryption() == linphone::MediaEncryption::DTLS);
		zrtp->endCurrentCall(dtls);

		// Call from DTLS to ZRTP
		auto dtlsCallParams = dtls->getCore()->createCallParams(nullptr);
		dtlsCallParams->setMediaEncryption(linphone::MediaEncryption::DTLS);
		dtls->call(zrtp, dtlsCallParams);
		BC_ASSERT_TRUE(zrtp->getCore()->getCurrentCall()->getCurrentParams()->getMediaEncryption() == linphone::MediaEncryption::ZRTP);
		BC_ASSERT_TRUE(dtls->getCore()->getCurrentCall()->getCurrentParams()->getMediaEncryption() == linphone::MediaEncryption::DTLS);
		zrtp->endCurrentCall(dtls);
	}
}

static void sdes2sdes256() {
	// initialize and start the proxy and B2bua server
	auto server = std::make_shared<B2buaServer>("/config/flexisip_b2bua.conf");
	{
		// Create and register clients
		auto sdes = std::make_shared<CoreClient>("sip:b2bua_srtp@sip.example.org", server);
		auto sdes256 = std::make_shared<CoreClient>("sip:b2bua_srtp256@sip.example.org", server);

		// Call from SDES to SDES256
		auto sdesCallParams = sdes->getCore()->createCallParams(nullptr);
		sdesCallParams->setMediaEncryption(linphone::MediaEncryption::SRTP);
		sdesCallParams->setSrtpSuites({linphone::SrtpSuite::AESCM128HMACSHA180, linphone::SrtpSuite::AESCM128HMACSHA132});
		sdes->call(sdes256, sdesCallParams);
		BC_ASSERT_TRUE(sdes->getCore()->getCurrentCall()->getCurrentParams()->getMediaEncryption() == linphone::MediaEncryption::SRTP);
		BC_ASSERT_TRUE(sdes->getCore()->getCurrentCall()->getCurrentParams()->getSrtpSuites().front() == linphone::SrtpSuite::AESCM128HMACSHA180);
		BC_ASSERT_TRUE(sdes256->getCore()->getCurrentCall()->getCurrentParams()->getMediaEncryption() == linphone::MediaEncryption::SRTP);
		BC_ASSERT_TRUE(sdes256->getCore()->getCurrentCall()->getCurrentParams()->getSrtpSuites().front() == linphone::SrtpSuite::AES256CMHMACSHA180);
		sdes->endCurrentCall(sdes256);

		// Call from SDES256 to SDES
		auto sdes256CallParams = sdes->getCore()->createCallParams(nullptr);
		sdes256CallParams->setMediaEncryption(linphone::MediaEncryption::SRTP);
		sdes256CallParams->setSrtpSuites({linphone::SrtpSuite::AES256CMHMACSHA180, linphone::SrtpSuite::AES256CMHMACSHA132});
		sdes256->call(sdes, sdes256CallParams);
		BC_ASSERT_TRUE(sdes->getCore()->getCurrentCall()->getCurrentParams()->getMediaEncryption() == linphone::MediaEncryption::SRTP);
		BC_ASSERT_TRUE(sdes->getCore()->getCurrentCall()->getCurrentParams()->getSrtpSuites().front() == linphone::SrtpSuite::AESCM128HMACSHA180);
		BC_ASSERT_TRUE(sdes256->getCore()->getCurrentCall()->getCurrentParams()->getMediaEncryption() == linphone::MediaEncryption::SRTP);
		BC_ASSERT_TRUE(sdes256->getCore()->getCurrentCall()->getCurrentParams()->getSrtpSuites().front() == linphone::SrtpSuite::AES256CMHMACSHA180);
		sdes->endCurrentCall(sdes256);
	}
}

static test_t tests[] = {
	TEST_NO_TAG("Basic", basic),
	TEST_NO_TAG("Forward Media Encryption", forward),
	TEST_NO_TAG("SDES to ZRTP call", sdes2zrtp),
	TEST_NO_TAG("SDES to DTLS call", sdes2dtls),
	TEST_NO_TAG("ZRTP to DTLS call", zrtp2dtls),
	TEST_NO_TAG("SDES to SDES256 call", sdes2sdes256),
};

} //namespace b2buatester
test_suite_t b2bua_suite = {
	"B2bua",
	NULL,
	NULL,
	NULL,
	NULL,
	sizeof(b2buatester::tests) / sizeof(b2buatester::tests[0]),
	b2buatester::tests
};
