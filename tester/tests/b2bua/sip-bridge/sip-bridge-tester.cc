/** Copyright (C) 2010-2024 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "b2bua/sip-bridge/external-provider-bridge.hh"

#include <chrono>

#include "b2bua/b2bua-server.hh"
#include "utils/client-builder.hh"
#include "utils/client-call.hh"
#include "utils/client-core.hh"
#include "utils/core-assert.hh"
#include "utils/proxy-server.hh"
#include "utils/string-formatter.hh"
#include "utils/temp-file.hh"
#include "utils/test-patterns/test.hh"
#include "utils/test-suite.hh"

namespace flexisip::tester {
namespace {

using namespace std::chrono_literals;

/*
    Test bridging to *and* from an external sip provider/domain. (Arbitrarily called "Jabiru")
    We configure 2 providers, one for each direction.

    The first, "Outbound" provider will attempt to find an external account matching the caller, and bridge the call
    using that account.
    The second, "Inbound" provider will attempt to find the external account that received the call to determine the uri
    to call in the internal domain, and send the invite to the flexisip proxy.

    We'll need a user registered to the internal Flexisip proxy. Let's call him Felix <sip:felix@flexisip.example.org>.
    Felix will need an account on the external Jabiru proxy, with a potentially different username than the one he uses
    on Flexisip: <sip:definitely-not-felix@jabiru.example.org>. That account will be provisioned in the B2BUA's account
    pool.
    Then we'll need a user registered to the Jabiru proxy, let's call him Jasper <sip:jasper@jabiru.example.org>.

    Felix will first attempt to call Jasper as if he was in the same domain as him, using the address
    <sip:jasper@flexisip.example.org>. Jasper should receive a bridged call coming from
    <sip:definitely-not-felix@jabiru.example.org>, Felix's external account managed by the B2BUA.

    Then Jasper will in turn attempt to call Felix's external account, <sip:definitely-not-felix@jabiru.example.org>,
    and Felix should receive a call form Jasper that should look like it's coming from within the same domain as him:
    <sip:jasper@flexisip.example.org>
*/
// TODO: There should really be 2 different proxies, to test that the Inbound provider can correctly send invites to the
// Flexisip proxy and not the `outboundProxy` configured on the B2BUA account.
void bidirectionalBridging() {
	StringFormatter jsonConfig{R"json({
		"schemaVersion": 2,
		"providers": [
			{
				"name": "Flexisip -> Jabiru (Outbound)",
				"triggerCondition": {
					"strategy": "Always"
				},
				"accountToUse": {
					"strategy": "FindInPool",
					"source": "{from}",
					"by": "alias"
				},
				"onAccountNotFound": "nextProvider",
				"outgoingInvite": {
					"to": "sip:{incoming.to.user}@{account.sipIdentity.hostport}{incoming.to.uriParameters}",
					"from": "{account.sipIdentity}"
				},
				"accountPool": "FlockOfJabirus"
			},
			{
				"name": "Jabiru -> Flexisip (Inbound)",
				"triggerCondition": {
					"strategy": "Always"
				},
				"accountToUse": {
					"strategy": "FindInPool",
					"source": "{to}",
					"by": "sipIdentity"
				},
				"onAccountNotFound": "nextProvider",
				"outgoingInvite": {
					"to": "{account.alias}",
					"from": "sip:{incoming.from.user}@{account.alias.hostport}{incoming.from.uriParameters}",
					"outboundProxy": "<sip:127.0.0.1:port;transport=tcp>"
				},
				"accountPool": "FlockOfJabirus"
			}
		],
		"accountPools": {
			"FlockOfJabirus": {
				"outboundProxy": "<sip:127.0.0.1:port;transport=tcp>",
				"registrationRequired": true,
				"maxCallsPerLine": 3125,
				"loader": [
					{
						"uri": "sip:definitely-not-felix@jabiru.example.org",
						"alias": "sip:felix@flexisip.example.org"
					}
				]
			}
		}
	})json",
	                           '', ''};
	TempFile providersJson{};
	Server proxy{{
	    // Requesting bind on port 0 to let the kernel find any available port
	    {"global/transports", "sip:127.0.0.1:0;transport=tcp"},
	    {"module::Registrar/enabled", "true"},
	    {"module::Registrar/reg-domains", "flexisip.example.org jabiru.example.org"},
	    {"b2bua-server/application", "sip-bridge"},
	    {"b2bua-server/transport", "sip:127.0.0.1:0;transport=tcp"},
	    {"b2bua-server::sip-bridge/providers", providersJson.name},
	}};
	proxy.start();
	providersJson.writeStream() << jsonConfig.format({{"port", proxy.getFirstPort()}});
	const auto b2buaLoop = std::make_shared<sofiasip::SuRoot>();
	const auto b2buaServer = std::make_shared<B2buaServer>(b2buaLoop);
	b2buaServer->init();
	ConfigManager::get()
	    ->getRoot()
	    ->get<GenericStruct>("module::Router")
	    ->get<ConfigString>("fallback-route")
	    ->set("sip:127.0.0.1:" + std::to_string(b2buaServer->getTcpPort()) + ";transport=tcp");
	proxy.getAgent()->findModule("Router")->reload();
	auto builder = proxy.clientBuilder();
	const auto felix = builder.build("felix@flexisip.example.org");
	const auto jasper = builder.build("jasper@jabiru.example.org");
	CoreAssert asserter{proxy, *b2buaLoop};
	asserter
	    .iterateUpTo(
	        3,
	        [&sipProviders =
	             dynamic_cast<const b2bua::bridge::SipBridge&>(b2buaServer->getApplication()).getProviders()] {
		        for (const auto& provider : sipProviders) {
			        for (const auto& [_, account] : provider.getAccountSelectionStrategy().getAccountPool()) {
				        FAIL_IF(!account->isAvailable());
			        }
		        }
		        // b2bua accounts registered
		        return ASSERTION_PASSED();
	        },
	        40ms)
	    .assert_passed();
	asserter.registerSteppable(felix);
	asserter.registerSteppable(jasper);

	// Flexisip -> Jabiru
	felix.invite("jasper@flexisip.example.org");
	BC_HARD_ASSERT(asserter
	                   .iterateUpTo(
	                       3,
	                       [&jasper] {
		                       const auto& current_call = jasper.getCurrentCall();
		                       FAIL_IF(current_call == std::nullopt);
		                       FAIL_IF(current_call->getState() != linphone::Call::State::IncomingReceived);
		                       // invite received
		                       return ASSERTION_PASSED();
	                       },
	                       300ms)
	                   .assert_passed());
	BC_ASSERT_CPP_EQUAL(jasper.getCurrentCall()->getRemoteAddress()->asStringUriOnly(),
	                    "sip:definitely-not-felix@jabiru.example.org");

	// cleanup
	jasper.getCurrentCall()->accept();
	jasper.getCurrentCall()->terminate();
	asserter
	    .iterateUpTo(
	        2,
	        [&felix] {
		        const auto& current_call = felix.getCurrentCall();
		        FAIL_IF(current_call != std::nullopt);
		        // invite received
		        return ASSERTION_PASSED();
	        },
	        90ms)
	    .assert_passed();

	// Jabiru -> Flexisip
	jasper.invite("definitely-not-felix@jabiru.example.org");
	BC_HARD_ASSERT(asserter
	                   .iterateUpTo(
	                       2,
	                       [&felix] {
		                       const auto& current_call = felix.getCurrentCall();
		                       FAIL_IF(current_call == std::nullopt);
		                       FAIL_IF(current_call->getState() != linphone::Call::State::IncomingReceived);
		                       // invite received
		                       return ASSERTION_PASSED();
	                       },
	                       400ms)
	                   .assert_passed());
	BC_ASSERT_CPP_EQUAL(felix.getCurrentCall()->getRemoteAddress()->asStringUriOnly(),
	                    "sip:jasper@flexisip.example.org");

	// shutdown / cleanup
	felix.getCurrentCall()->accept();
	felix.getCurrentCall()->terminate();
	asserter
	    .iterateUpTo(
	        2,
	        [&jasper] {
		        const auto& current_call = jasper.getCurrentCall();
		        FAIL_IF(current_call != std::nullopt);
		        // invite received
		        return ASSERTION_PASSED();
	        },
	        400ms)
	    .assert_passed();
	b2buaServer->stop();
}

TestSuite _{
    "b2bua::bridge",
    {
        CLASSY_TEST(bidirectionalBridging),
    },
};
} // namespace
} // namespace flexisip::tester
