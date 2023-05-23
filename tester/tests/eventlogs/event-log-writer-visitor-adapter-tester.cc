/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "eventlogs/event-log-writer-visitor-adapter.hh"

#include <memory>
#include <sstream>

#include "bctoolbox/tester.h"
#include "flexisip/sofia-wrapper/msg-sip.hh"

#include "eventlogs/call-ended-event-log.hh"
#include "eventlogs/call-ringing-event-log.hh"
#include "eventlogs/call-started-event-log.hh"
#include "eventlogs/eventlogs.hh"
#include "sofia-wrapper/sip-header-private.hh"
#include "utils/test-patterns/test.hh"
#include "utils/test-suite.hh"
#include "utils/variant-utils.hh"

namespace {
using namespace flexisip;
using namespace flexisip::tester;
using namespace std;

void logMessage() {
	sofiasip::MsgSip msg{};
	msg.makeAndInsert<sofiasip::SipHeaderFrom>("msg-event-log-test-from@example.org");
	msg.makeAndInsert<sofiasip::SipHeaderTo>("msg-event-log-test-to@example.org");
	msg.makeAndInsert<sofiasip::SipHeaderUserAgent>("msg-event-log-test-user-agent");
	msg.makeAndInsert<sofiasip::SipHeaderCallID>();
	auto messageLog = make_shared<MessageLog>(msg.getSip(), MessageLog::ReportType::DeliveredToUser);
	EventLogWriterVisitorAdapter logWriter{overloaded{
	    [&messageLog](const MessageLog& log) { BC_ASSERT_CPP_EQUAL(&log, messageLog.get()); },
	    [](const auto& log) {
		    ostringstream msg{};
		    msg << "This test is not supposed to write a " << typeid(log).name();
		    BC_HARD_FAIL(msg.str().c_str());
	    },
	}};

	static_cast<EventLogWriter&>(logWriter).write(messageLog);
}

TestSuite _("EventLogWriterVisitorAdapter",
            {
                CLASSY_TEST(logMessage),
            });
} // namespace
