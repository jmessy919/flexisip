/** Copyright (C) 2010-2024 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "b2bua/sip-bridge/invite-tweaker.hh"

#include <linphone/misc.h>

#include "utils/client-builder.hh"
#include "utils/client-call.hh"
#include "utils/client-core.hh"
#include "utils/injected-module.hh"
#include "utils/test-patterns/test.hh"
#include "utils/test-suite.hh"

namespace flexisip::tester {
namespace {
using namespace flexisip::b2bua::bridge;
using namespace std::chrono_literals;

void test() {
	const SipUri expectedToAddress{"sip:expected-to@to.example.org;custom-param=To"};
	InjectedHooks hooks{{
	    .onRequest =
	        [&expectedToAddress](const std::shared_ptr<RequestSipEvent>& requestEvent) {
		        const auto* sip = requestEvent->getSip();
		        // Mangle To header
		        sip->sip_to->a_url[0] = *expectedToAddress.get();
	        },
	}};
	Server proxy{
	    {
	        // Requesting bind on port 0 to let the kernel find any available port
	        {"global/transports", "sip:127.0.0.1:0;transport=tcp"},
	        {"module::Registrar/enabled", "true"},
	        {"module::Registrar/reg-domains", "sip.example.org"},
	    },
	    &hooks,
	};
	proxy.start();
	const auto& builder = proxy.clientBuilder();
	const auto& caller = builder.build("sip:expected-from@sip.example.org;custom-param=From");
	const auto& b2bua = builder.build("sip:expected-request-uri@sip.example.org;custom-param=RequestUri");
	caller.invite(b2bua);
	BC_HARD_ASSERT_TRUE(b2bua.hasReceivedCallFrom(caller));
	const auto forgedCall = ClientCall::getLinphoneCall(*b2bua.getCurrentCall());
	const auto& requestAddress = forgedCall->getRequestAddress();
	BC_HARD_ASSERT(requestAddress != nullptr);
	BC_ASSERT_CPP_EQUAL(requestAddress->getUsername(), "expected-request-uri");
	BC_ASSERT_CPP_EQUAL(requestAddress->getUriParam("custom-param"), "RequestUri");
	const auto& toAddress = forgedCall->getToAddress();
	BC_HARD_ASSERT(toAddress != nullptr);
	BC_ASSERT_CPP_EQUAL(toAddress->getUsername(), "expected-to");
	BC_ASSERT_CPP_EQUAL(toAddress->getUriParam("custom-param"), "To");
	const auto& fromAddress = forgedCall->getRemoteAddress();
	BC_ASSERT_CPP_EQUAL(fromAddress->getUsername(), "expected-from");
	BC_ASSERT_CPP_EQUAL(fromAddress->getUriParam("custom-param"), "From");
	Account forgedAccount{b2bua.getAccount(), 0x7E57};

	{
		const auto& outgoingCallParams = b2bua.getCore()->createCallParams(forgedCall);
		const auto& toAddress = InviteTweaker{{.to = "sip:{incoming.requestAddress.user}@stub.example.org"}}.tweakInvite(*forgedCall, forgedAccount,
		                                                                    *outgoingCallParams);
		BC_ASSERT_CPP_EQUAL(toAddress->asStringUriOnly(), "sip:expected-request-uri@stub.example.org");
	}
}

TestSuite _{
    "b2bua::sip-bridge::InviteTweaker",
    {
        CLASSY_TEST(test),
    },
};
} // namespace
} // namespace flexisip::tester
