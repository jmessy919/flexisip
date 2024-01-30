/** Copyright (C) 2010-2024 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <unordered_set>

#include "b2bua/sip-bridge/accounts/loaders/static-account-loader.hh"
#include "utils/test-patterns/test.hh"
#include "utils/test-suite.hh"

namespace flexisip::tester {
using namespace std;
using namespace flexisip::b2bua::bridge;
using namespace flexisip::b2bua::bridge::config::v2;

void nominalInitialLoadTest() {
	// Static loader only move parsed config, this is a trivial test.

	auto loaderConfig = R"([
			{
				"uri": "sip:account1@some.provider.example.com",
				"alias": "sip:expected-from@sip.example.org"
			},
			{
				"uri": "sip:account2@some.provider.example.com",
				"userid": "userID",
				"password": "p@$sword",
				"outboundProxy": "sip.linphone.org"
			}
		]
	)"_json.get<config::v2::StaticLoader>();

	auto expectedAccounts = loaderConfig;

	StaticAccountLoader loader{std::move(loaderConfig)};

	auto actualAccounts = loader.initialLoad();

	BC_ASSERT_CPP_EQUAL(expectedAccounts, actualAccounts);
}

namespace {
TestSuite _{
    "Static account loader unit tests",
    {
        CLASSY_TEST(nominalInitialLoadTest),
    },
};
} // namespace
} // namespace flexisip::tester
