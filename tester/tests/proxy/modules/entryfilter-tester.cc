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

#include "sofia-wrapper/nta-agent.hh"
#include "tester.hh"
#include "utils/core-assert.hh"
#include "utils/proxy-server.hh"
#include "utils/string-formatter.hh"
#include "utils/test-patterns/test.hh"
#include "utils/test-suite.hh"

namespace {

using namespace flexisip;
using namespace flexisip::tester;
using namespace std::string_literals;
using namespace std::chrono_literals;

void filtersApplyToAllContactsInRegister() {
	constexpr auto matchingContact = "<sip:matching@example.org>";
	constexpr auto filteredOutContact = "<sip:filtered-out@example.org>";
	auto received = false;
	auto hooks = InjectedHooks{
	    .onRequest = [&received](auto) { received = true; },
	};
	auto proxy = Server(
	    {
	        // Requesting bind on port 0 to let the kernel find any available port
	        {"global/transports", "sip:127.0.0.1:0;transport=udp"},
	        {"module::InjectedTestModule/filter", "contact.uri.user regex '^matching$'"},
	    },
	    &hooks);
	proxy.start();
	auto asserter = CoreAssert(proxy);
	const auto& requestTemplate = StringFormatter{
	    "REGISTER sip:stub.example.org SIP/2.0\r\n"
	    "From: <sip:stub@example.org>;tag=stub\r\n"
	    "To: <sip:stub@example.org>\r\n"
	    "Call-ID: Call-ID\r\n"
	    "CSeq: 1 REGISTER\r\n"
	    "Contact: Contact1\r\n"
	    "Contact: Contact2\r\n"
	    "Max-Forwards: 42\r\n"
	    "Content-Length: 0\r\n\r\n",
	    '',
	    '',
	};
	auto sender = sofiasip::NtaAgent(proxy.getRoot(), "sip:localhost:0");
	const auto& destination = "sip:127.0.0.1:"s + proxy.getFirstPort();
	const auto& registerMatched = [&](auto&& firstContact, auto&& secondContact) {
		received = false;
		sender.createOutgoingTransaction(requestTemplate.format({
		                                     {"Call-ID", randomString(6)},
		                                     {"Contact1", firstContact},
		                                     {"Contact2", secondContact},
		                                 }),
		                                 destination);

		return asserter
		    .iterateUpTo(
		        1, [&received]() { return LOOP_ASSERTION(received); }, 20ms)
		    .assert_passed();
	};

	BC_ASSERT(registerMatched(matchingContact, matchingContact));
	BC_ASSERT(registerMatched(filteredOutContact, matchingContact));
	BC_ASSERT(registerMatched(matchingContact, filteredOutContact));
}

TestSuite _("EntryFilter",
            {
                CLASSY_TEST(filtersApplyToAllContactsInRegister),
            });
} // namespace
