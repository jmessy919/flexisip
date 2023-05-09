/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <vector>

#include "bctoolbox/tester.h"
#include "eventlogs/event-log-utils.hh"
#include "flexisip/module-router.hh"
#include "sofia-sip/sip.h"

#include "eventlogs/call-started-event-log.hh"
#include "eventlogs/call-ringing-event-log.hh"
#include "eventlogs/event-log-writer-visitor-adapter.hh"
#include "eventlogs/event-log-writer.hh"
#include "eventlogs/eventlogs.hh"
#include "registrar/extended-contact.hh"
#include "utils/client-core.hh"
#include "utils/proxy-server.hh"
#include "utils/test-patterns/test.hh"
#include "utils/test-suite.hh"
#include "utils/variant-utils.hh"

namespace {
using namespace flexisip;
using namespace flexisip::tester;
using namespace std;

string toString(const sip_from_t* from) {
	return eventlogs::sipDataToString(from->a_url);
}

void callStartedAndEnded() {
	const auto proxy = make_shared<Server>(map<string, string>{
	    // Requesting bind on port 0 to let the kernel find any available port
	    {"global/transports", "sip:127.0.0.1:0;transport=tcp"},
	    {"module::Registrar/enabled", "true"},
	    {"module::Registrar/reg-domains", "sip.example.org"},
	    {"module::MediaRelay/enabled", "true"},
	    {"module::MediaRelay/prevent-loops", "false"}, // Allow loopback to localnetwork
	});
	proxy->start();
	const auto& agent = proxy->getAgent();
	const auto before = chrono::system_clock::now();
	vector<CallStartedEventLog> callsStarted{};
	vector<CallLog> invitesEnded{};
	const string expectedFrom = "sip:tony@sip.example.org";
	const string expectedTo = "sip:mike@sip.example.org";
	agent->setEventLogWriter(unique_ptr<EventLogWriter>(new EventLogWriterVisitorAdapter{overloaded{
	    [&callsStarted](const CallStartedEventLog& call) {
		    // SAFETY force-moving is OK as long as the event is not accessed after this callback
		    callsStarted.emplace_back(move(const_cast<CallStartedEventLog&>(call)));
	    },
	    [&invitesEnded](const CallLog& call) {
		    // SAFETY force-moving is OK as long as the event is not accessed after this callback
		    invitesEnded.emplace_back(move(const_cast<CallLog&>(call)));
	    },
	    [](const RegistrationLog&) { /* ignored */ },
	    [](const auto& log) {
		    ostringstream msg{};
		    msg << "This test is not supposed to write a " << typeid(log).name();
		    BC_HARD_FAIL(msg.str().c_str());
	    },
	}}));
	auto tony = ClientBuilder(expectedFrom).registerTo(proxy);
	auto mike = ClientBuilder(expectedTo).registerTo(proxy);

	const auto call = tony.call(mike);

	BC_ASSERT_CPP_EQUAL(callsStarted.size(), 1);
	BC_ASSERT_CPP_EQUAL(invitesEnded.size(), 1);
	const auto& startedEvent = callsStarted[0];
	BC_ASSERT_TRUE(before < startedEvent.mInitiatedAt);
	BC_ASSERT_CPP_EQUAL(toString(startedEvent.mFrom), expectedFrom);
	BC_ASSERT_CPP_EQUAL(toString(startedEvent.mTo), expectedTo);
	BC_ASSERT_CPP_EQUAL(startedEvent.mDevices.size(), 1);
	const string_view deviceKey = startedEvent.mDevices[0].mKey.str();
	BC_ASSERT_CPP_EQUAL(
	    deviceKey.substr(sizeof("\"<urn:uuid:") - 1, sizeof("00000000-0000-0000-0000-000000000000") - 1),
	    mike.getCore()->getConfig()->getString("misc", "uuid", "UNSET!"));
	const string eventId = startedEvent.mId;
	const auto& acceptedEvent = invitesEnded[0];
	BC_ASSERT_CPP_EQUAL(toString(acceptedEvent.getFrom()), expectedFrom);
	BC_ASSERT_CPP_EQUAL(toString(acceptedEvent.getTo()), expectedTo);
	BC_ASSERT_CPP_EQUAL(string(acceptedEvent.mId), eventId);
	BC_ASSERT_TRUE(acceptedEvent.mDevice != nullopt);
	BC_ASSERT_CPP_EQUAL(acceptedEvent.mDevice->mKey.str(), deviceKey);
}

TestSuite _("EventLog Stats",
            {
                CLASSY_TEST(callStartedAndEnded),
            });
} // namespace
