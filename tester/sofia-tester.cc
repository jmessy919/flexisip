/*
    Flexisip, a flexible SIP proxy server with media capabilities.
    Copyright (C) 2010-2024 Belledonne Communications SARL, All rights reserved.

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

#include <future>

#include "sofia-sip/http.h"
#include "sofia-sip/nta.h"
#include "sofia-sip/nta_stateless.h"
#include "sofia-sip/nth.h"
#include "sofia-sip/tport_tag.h"

#include "sofia-wrapper/nta-agent.hh"
#include "sofia-wrapper/sip-header-private.hh"

#include "flexisip/logmanager.hh"
#include "flexisip/sofia-wrapper/su-root.hh"

#include "utils/test-patterns/test.hh"
#include "utils/test-suite.hh"
#include "utils/tls-server.hh"

using namespace std;
using namespace sofiasip;

namespace flexisip::tester {
namespace {

/*
 * Test sofia-SIP nth_engine, with TLS SNI enabled/disabled.
 */
template <bool enabled>
void nthEngineWithSni() {
	SuRoot root{};
	TlsServer server{};
	bool requestReceived = false;
	auto requestMatch = async(launch::async, [&server, &requestReceived, sni = enabled]() {
		server.accept(sni ? "127.0.0.1" : ""); // SNI checks are done in TlsServer::accept.
		server.read();
		server.send("Status: 200");
		return requestReceived = true;
	});

	const auto url = "https://127.0.0.1:" + to_string(server.getPort());
	auto* engine = nth_engine_create(root.getCPtr(), TPTAG_TLS_SNI(enabled), TAG_END());

	nth_client_t* request = nth_client_tcreate(
	    engine,
	    []([[maybe_unused]] nth_client_magic_t* magic, [[maybe_unused]] nth_client_t* request,
	       [[maybe_unused]] const http_t* http) { return 0; },
	    nullptr, http_method_get, "GET", URL_STRING_MAKE(url.c_str()), TAG_END());

	if (request == nullptr) {
		BC_FAIL("No request sent.");
	}

	while (!requestReceived) {
		root.step(10ms);
	}

	BC_ASSERT_TRUE(requestMatch.get());
	nth_client_destroy(request);
	nth_engine_destroy(engine);
}

/*
 * Test behavior of sofia-SIP when data read from socket [is under/exceeds/equals] agent's message maxsize.
 * 1. Send several requests to the UAS.
 * 2. Iterate on the main loop, so the UAS will collect pending requests from the socket.
 * 3. UAS should process all collected data even if the number of data (in bytes) exceeds agent's message maxsize.
 *
 * Generated requests have a size of 322 bytes.
 * Agent's message maxsize is set to 4500 bytes. It is higher than fallback value of 4096 bytes used in
 * sofia-SIP function: tport_recv_iovec.
 * 10 * 322 = 3220
 * 15 * 322 = 4830
 * 20 * 322 = 6440
 * 40 * 322 = 12880
 */
template <int maxsize, int nbRequests, const string& transport>
void collectAndParseDataFromSocket() {
	static int expectedStatus = 202;
	struct TestHelper {
		struct SipIdentity {
			string user{};
			string host{};
			string str() {
				return "sip:" + user + "@" + host;
			}
		};

		int processedRequests = 0;
		SipIdentity stubIdentity{.user = "stub-user", .host = "localhost"};
	};
	TestHelper helper{};

	// Function called on request processing.
	auto callback = [](nta_agent_magic_t* context, nta_agent_t* agent, msg_t* msg, sip_t* sip) -> int {
		if (sip and sip->sip_request and sip->sip_request->rq_method == sip_method_register) {
			BC_HARD_ASSERT(sip->sip_contact != nullptr);
			auto* helper = reinterpret_cast<TestHelper*>(context);
			BC_HARD_ASSERT_CPP_EQUAL(sip->sip_contact->m_url->url_user, helper->stubIdentity.user);
			BC_HARD_ASSERT_CPP_EQUAL(sip->sip_contact->m_url->url_host, helper->stubIdentity.host);
			++helper->processedRequests;
		}
		nta_msg_treply(agent, msg, expectedStatus, "Accepted", TAG_END()); // Complete generated outgoing transactions.
		return 0;
	};

	auto suRoot = make_shared<SuRoot>();
	NtaAgent server{
	    suRoot,
	    "sip:127.0.0.1:0;" + transport,
	    callback,
	    reinterpret_cast<nta_agent_magic_t*>(&helper),
	    NTATAG_MAXSIZE(maxsize),
	    TAG_END(),
	};
	NtaAgent client{suRoot, "sip:127.0.0.1:0;" + transport, nullptr, nullptr, NTATAG_UA(false), TAG_END()};

	// Send requests to UAS.
	list<shared_ptr<NtaOutgoingTransaction>> transactions{};
	const auto routeUri = "sip:127.0.0.1:"s + server.getPort() + ";" + transport;
	for (int requestId = 0; requestId < nbRequests; ++requestId) {
		auto request = make_unique<MsgSip>();
		request->makeAndInsert<SipHeaderRequest>(sip_method_register, "sip:localhost");
		request->makeAndInsert<SipHeaderFrom>(helper.stubIdentity.str(), "stub-from-tag");
		request->makeAndInsert<SipHeaderTo>(helper.stubIdentity.str());
		request->makeAndInsert<SipHeaderCallID>("stub-call-id");
		request->makeAndInsert<SipHeaderCSeq>(20u + requestId, sip_method_register);
		request->makeAndInsert<SipHeaderContact>("<" + helper.stubIdentity.str() + ";" + transport + ">");
		request->makeAndInsert<SipHeaderExpires>(10);

		transactions.push_back(client.createOutgoingTransaction(std::move(request), routeUri));
	}

	// Iterate on main loop.
	const auto timeout = chrono::system_clock::now() + 100ms;
	while (chrono::system_clock::now() < timeout) {
		suRoot->step(10ms);
	}

	BC_ASSERT_CPP_EQUAL(helper.processedRequests, nbRequests);
	for (const auto& transaction : transactions) {
		BC_ASSERT(transaction->isCompleted());
		BC_ASSERT_CPP_EQUAL(transaction->getStatus(), expectedStatus);
	}
}

/*
 * Test parsing of a SIP message whose size exceeds msg maxsize.
 *
 * Sofia-SIP cannot parse a SIP message that exceeds the maximum acceptable size of an incoming message.
 */
void collectAndTryToParseSIPMessageThatExceedsMsgMaxsize() {
	int expectedStatus = 0;
	auto suRoot = make_shared<SuRoot>();
	NtaAgent server{
	    suRoot, "sip:127.0.0.1:0;transport=tcp", nullptr, nullptr, NTATAG_MAXSIZE(128), TAG_END(),
	};
	NtaAgent client{suRoot, "sip:127.0.0.1:0;transport=tcp", nullptr, nullptr, NTATAG_UA(false), TAG_END()};

	// Send requests to UAS.
	const auto routeUri = "sip:127.0.0.1:"s + server.getPort() + ";transport=tcp";
	auto request = make_unique<MsgSip>();
	request->makeAndInsert<SipHeaderRequest>(sip_method_register, "sip:localhost");
	request->makeAndInsert<SipHeaderFrom>("sip:stub-user@localhost", "stub-from-tag");
	request->makeAndInsert<SipHeaderTo>("sip:stub-user@localhost");
	request->makeAndInsert<SipHeaderCallID>("stub-call-id");
	request->makeAndInsert<SipHeaderCSeq>(20u, sip_method_register);
	request->makeAndInsert<SipHeaderContact>("<sip:stub-user@localhost;transport=tcp>");
	request->makeAndInsert<SipHeaderExpires>(10);

	auto transaction = client.createOutgoingTransaction(std::move(request), routeUri);

	// Iterate on main loop.
	const auto timeout = chrono::system_clock::now() + 100ms;
	while (chrono::system_clock::now() < timeout) {
		suRoot->step(10ms);
	}

	BC_ASSERT(!transaction->isCompleted());
	BC_ASSERT_CPP_EQUAL(transaction->getStatus(), expectedStatus);
}

const auto TCP = "transport=tcp"s;
const auto TLS = "transport=tls"s;

TestSuite _("Sofia-SIP",
            {
                CLASSY_TEST(nthEngineWithSni<true>),
                CLASSY_TEST(nthEngineWithSni<false>),
                CLASSY_TEST((collectAndParseDataFromSocket<4096, 10, TCP>)), // message size under maxsize
                CLASSY_TEST((collectAndParseDataFromSocket<3220, 10, TCP>)), // message size equals maxsize
                CLASSY_TEST((collectAndParseDataFromSocket<4096, 20, TCP>)), // message size above maxsize
                CLASSY_TEST((collectAndParseDataFromSocket<4096, 40, TCP>)), // message size +2x above maxsize
                CLASSY_TEST(collectAndTryToParseSIPMessageThatExceedsMsgMaxsize),
            });

} // namespace
} // namespace flexisip::tester