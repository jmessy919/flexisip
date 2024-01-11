/** Copyright (C) 2010-2024 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "b2bua/sip-bridge/configuration/v2.hh"

#include "utils/test-patterns/test.hh"
#include "utils/test-suite.hh"

namespace flexisip::tester {
namespace {
using namespace b2bua::bridge::config;

void v1ConfigExpressedAsEquivalentV2Config() {
	const auto j = R"json({
  "schemaVersion": 2,
  "providers": [
    {
      "name": "Pattern matching (legacy) provider, new style",
      "outboundProxy": "<sip:some.provider.example.com;transport=tls>",
      "registrationRequired": true,
      "maxCallsPerLine": 500,
      "triggerCondition": {
        "source": "${incoming.from}",
        "strategy": "MatchRegex",
        "pattern": "sip:+33.*"
      },
      "accountToUse": {
        "strategy": "Random"
      },
      "onAccountNotFound": "decline",
      "outgoingInvite": {
        "to": "sip:${incoming.requestAddress.userinfo}@${account.sipIdentity.hostport}${incoming.requestAddress.uriParameters}"
      },
      "accountPool": "MyIncredibleTestAccountPool"
    }
  ],
  "accountPools": {
    "MyIncredibleTestAccountPool": [
      {
        "uri": "sip:account1@some.provider.example.com",
        "userid": "userid1",
        "password": "correct horse battery staple",
        "alias": "sip:alias@internal.domain.example.com"
      },
      {
        "uri": "sip:account2@some.provider.example.com",
        "password": "secret horse battery staple"
      }
    ]
  }
})json"_json;

	auto deserialized = j.get<v2::Root>();

	BC_ASSERT_CPP_EQUAL(deserialized.schemaVersion, 2);
	BC_HARD_ASSERT_CPP_EQUAL(deserialized.accountPools.size(), 1);
	BC_ASSERT_CPP_EQUAL(deserialized.accountPools.begin()->first, "MyIncredibleTestAccountPool");
	const auto& staticPool = std::get<v2::StaticPool>(deserialized.accountPools.begin()->second);
	BC_HARD_ASSERT_CPP_EQUAL(staticPool.size(), 2);
	BC_ASSERT_CPP_EQUAL(staticPool[0].uri, "sip:account1@some.provider.example.com");
	BC_ASSERT_CPP_EQUAL(staticPool[0].userid, "userid1");
	BC_ASSERT_CPP_EQUAL(staticPool[0].password, "correct horse battery staple");
	BC_ASSERT_CPP_EQUAL(staticPool[0].alias, "sip:alias@internal.domain.example.com");
	BC_ASSERT_CPP_EQUAL(staticPool[1].uri, "sip:account2@some.provider.example.com");
	BC_ASSERT_CPP_EQUAL(staticPool[1].password, "secret horse battery staple");
	BC_ASSERT_CPP_EQUAL(staticPool[1].userid, "");
	BC_ASSERT_CPP_EQUAL(staticPool[1].alias, "");
	BC_HARD_ASSERT_CPP_EQUAL(deserialized.providers.size(), 1);
	BC_ASSERT_CPP_EQUAL(deserialized.providers[0].name, "Pattern matching (legacy) provider, new style");
	BC_ASSERT_CPP_EQUAL(deserialized.providers[0].outboundProxy, "<sip:some.provider.example.com;transport=tls>");
	BC_ASSERT_CPP_EQUAL(deserialized.providers[0].registrationRequired, true);
	BC_ASSERT_CPP_EQUAL(deserialized.providers[0].maxCallsPerLine, 500);
	const auto& matchRegex = std::get<v2::trigger_cond::MatchRegex>(deserialized.providers[0].triggerCondition);
	BC_ASSERT_CPP_EQUAL(matchRegex.source, "${incoming.from}");
	BC_ASSERT_CPP_EQUAL(matchRegex.pattern, "sip:+33.*");
	std::ignore = std::get<v2::account_selection::Random>(deserialized.providers[0].accountToUse);
	BC_ASSERT_ENUM_EQUAL(deserialized.providers[0].onAccountNotFound, v2::OnAccountNotFound::Decline);
	BC_ASSERT_CPP_EQUAL(deserialized.providers[0].accountPool, "MyIncredibleTestAccountPool");
}

TestSuite _("b2bua::sip-bridge::configuration::v2",
            {
                CLASSY_TEST(v1ConfigExpressedAsEquivalentV2Config),
            });
} // namespace
} // namespace flexisip::tester
