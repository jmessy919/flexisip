/** Copyright (C) 2010-2024 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <soci/session.h>
#include <soci/sqlite3/soci-sqlite3.h>

#include "b2bua/sip-bridge/accounts/loaders/sql-account-loader.hh"
#include "soci-helper.hh"
#include "utils/core-assert.hh"
#include "utils/string-formatter.hh"
#include "utils/test-patterns/test.hh"
#include "utils/test-suite.hh"
#include "utils/tmp-dir.hh"

namespace flexisip::tester {
using namespace std;
using namespace soci;
using namespace nlohmann;
using namespace flexisip::b2bua::bridge;
using namespace flexisip::b2bua::bridge::config::v2;

struct SuiteScope {
	const TmpDir tmpDir{"tmpDirForSqlLoader"};
	const std::string tmpDbFileName = tmpDir.path().string() + "/database_filename";
};

std::optional<SuiteScope> SUITE_SCOPE;

void nominalInitialSqlLoadTest() {
	auto expectedAccounts = R"([
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
	)"_json.get<std::vector<Account>>();

	// clang-format off
	auto sqlLoaderConf = nlohmann::json::parse(StringFormatter{
		R"({
			"dbBackend": "sqlite3",
			"initQuery": "SELECT usernameInDb as username, domain as hostport, userid as user_id, passwordInDb as password, alias_username, alias_domain as alias_hostport, outboundProxyInDb as outbound_proxy from users",
			"updateQuery": "not tested here",
			"connection": "@database_filename@"
		}
	)",'@', '@'}
	.format({{"database_filename", SUITE_SCOPE->tmpDbFileName}}))
	.get<SQLLoader>();
	// clang-format on

	SQLAccountLoader loader{make_shared<sofiasip::SuRoot>(), sqlLoaderConf};
	auto actualAccounts = loader.initialLoad();

	BC_ASSERT_CPP_EQUAL(expectedAccounts, actualAccounts);
}

void initialSqlLoadTestWithEmptyFields() {
	auto expectedAccounts = R"([
			{
				"uri": "sip:account1@some.provider.example.com",
				"alias": "sip:expected-from@sip.example.org"
			},
			{
				"uri": "sip:account2@some.provider.example.com"
			}
		]
	)"_json.get<std::vector<Account>>();

	// clang-format off
	auto sqlLoaderConf = nlohmann::json::parse(StringFormatter{
	    R"({
			"dbBackend": "sqlite3",
			"initQuery": "SELECT usernameInDb as username, domain as hostport,\"\" as user_id, \"\" as password, alias_username, alias_domain as alias_hostport, NULL as outbound_proxy from users",
			"updateQuery": "not tested here",
			"connection": "@database_filename@"
		}
	)",'@', '@'}
	.format({{"database_filename", SUITE_SCOPE->tmpDbFileName}}))
	.get<SQLLoader>();
	// clang-format on

	SQLAccountLoader loader{make_shared<sofiasip::SuRoot>(), sqlLoaderConf};

	auto actualAccounts = loader.initialLoad();

	BC_ASSERT_CPP_EQUAL(expectedAccounts, actualAccounts);
}

void initialSqlLoadTestUriCantBeNull() {
	// clang-format off
	auto sqlLoaderConf = nlohmann::json::parse(StringFormatter{
	    R"({
			"dbBackend": "sqlite3",
			"initQuery": "SELECT NULL as username, \"\" as hostport,  \"\" as user_id, \"\" as password, alias_username, alias_domain as alias_hostport, NULL as outbound_proxy from users",
			"updateQuery": "not tested here",
			"connection": "@database_filename@"
		}
	)",'@', '@'}
	.format({{"database_filename", SUITE_SCOPE->tmpDbFileName}}))
	.get<SQLLoader>();
	// clang-format on

	SQLAccountLoader loader{make_shared<sofiasip::SuRoot>(), sqlLoaderConf};
	BC_ASSERT_THROWN(loader.initialLoad(), SociHelper::DatabaseException)
}

void nominalUpdateSqlTest() {
	auto suRoot = make_shared<sofiasip::SuRoot>();
	// clang-format off
	auto sqlLoaderConf = nlohmann::json::parse(StringFormatter{
	    R"({
			"dbBackend": "sqlite3",
			"initQuery": "not tested here",
			"updateQuery": "SELECT usernameInDb as username, domain as hostport, userid as user_id, passwordInDb as password, alias_username, alias_domain as alias_hostport, outboundProxyInDb as outbound_proxy from users where usernameInDb = :username AND domain = :domain AND user_id = :identifier",
			"connection": "@database_filename@"
		}
	)",'@', '@'}
	.format({{"database_filename", SUITE_SCOPE->tmpDbFileName}}))
	.get<SQLLoader>();
	// clang-format on

	SQLAccountLoader loader{suRoot, sqlLoaderConf};

	Account actualAccount;
	loader.accountUpdateNeeded("account2", "some.provider.example.com", "userID",
	                           [&actualAccount](const Account& actualAccountCb) { actualAccount = actualAccountCb; });

	auto expectedAccount = R"(
				{
					"uri": "sip:account2@some.provider.example.com",
					"userid": "userID",
					"password": "p@$sword",
					"outboundProxy": "sip.linphone.org"
				}
		)"_json.get<Account>();

	CoreAssert asserter{*suRoot};
	asserter
	    .wait([&actualAccount, &expectedAccount] {
		    FAIL_IF(actualAccount != expectedAccount);
		    return ASSERTION_PASSED();
	    })
	    .assert_passed();
	BC_HARD_ASSERT_CPP_EQUAL(actualAccount, expectedAccount);
}

namespace {
const TestSuite _{
    "b2bua::bridge::account::SQLAccountLoader",
    {
        CLASSY_TEST(nominalInitialSqlLoadTest),
        CLASSY_TEST(initialSqlLoadTestWithEmptyFields),
        CLASSY_TEST(initialSqlLoadTestUriCantBeNull),
        CLASSY_TEST(nominalUpdateSqlTest),
    },
    Hooks()
        .beforeSuite([] {
	        SUITE_SCOPE.emplace();
	        try {
		        session sql(sqlite3, SUITE_SCOPE->tmpDbFileName);
		        sql << R"sql(CREATE TABLE users (
						usernameInDb TEXT PRIMARY KEY,
						domain TEXT,
						userid TEXT,
						passwordInDb TEXT,
						alias_username TEXT,
						alias_domain TEXT,
						outboundProxyInDb TEXT))sql";
		        sql << R"sql(INSERT INTO users VALUES ("account1", "some.provider.example.com", "", "", "expected-from", "sip.example.org", ""))sql";
		        sql << R"sql(INSERT INTO users VALUES ("account2", "some.provider.example.com", "userID", "p@$sword", "", "", "sip.linphone.org"))sql";
	        } catch (const soci_error& e) {
		        auto msg = "Error initiating DB : "s + e.what();
		        BC_HARD_FAIL(msg.c_str());
	        }
	        return 0;
        })
        .afterSuite([] {
	        SUITE_SCOPE.reset();
	        return 0;
        })};

} // namespace
} // namespace flexisip::tester
