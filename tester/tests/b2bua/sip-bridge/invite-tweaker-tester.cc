/** Copyright (C) 2010-2024 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "b2bua/sip-bridge/invite-tweaker.hh"

#include <linphone/misc.h>

#include "utils/client-core.hh"
#include "utils/core-assert.hh"
#include "utils/test-patterns/test.hh"
#include "utils/test-suite.hh"

namespace flexisip::tester {
namespace {
using namespace flexisip::b2bua::bridge;
using namespace std::chrono_literals;

void test() {
	const auto& factory = linphone::Factory::get();
	const auto& schizoCore = tester::minimalCore(*factory);
	const auto& transports = schizoCore->getTransports();
	transports->setTcpPort(LC_SIP_TRANSPORT_RANDOM);
	schizoCore->setTransports(transports);
	schizoCore->start();
	schizoCore->invite("sip:localhost:" + std::to_string(schizoCore->getTransportsUsed()->getTcpPort()));
	CoreAssert asserter{schizoCore};
	std::shared_ptr<linphone::Call> incomingCall = nullptr;
	BC_HARD_ASSERT(
	    asserter
	        .iterateUpTo(
	            1, [&schizoCore, &incomingCall]() { return bool(incomingCall = schizoCore->getCurrentCall()); }, 1s)
	        .assert_passed());

	InviteTweaker inviteTweaker{{.to = ""}};
}

TestSuite _{
    "b2bua::sip-bridge::InviteTweaker",
    {
        CLASSY_TEST(test),
    },
};
} // namespace
} // namespace flexisip::tester
