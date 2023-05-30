/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <unordered_map>
#include <utility>
#include <vector>

#include "bctoolbox/tester.h"
#include "flexisip/module-router.hh"
#include "linphone++/enums.hh"
#include "sofia-sip/sip.h"

#include "eventlogs/call-ended-event-log.hh"
#include "eventlogs/call-ringing-event-log.hh"
#include "eventlogs/call-started-event-log.hh"
#include "eventlogs/event-log-utils.hh"
#include "eventlogs/event-log-writer-visitor-adapter.hh"
#include "eventlogs/event-log-writer.hh"
#include "eventlogs/eventlogs.hh"
#include "fork-context/fork-status.hh"
#include "registrar/extended-contact.hh"
#include "utils/asserts.hh"
#include "utils/client-core.hh"
#include "utils/core-assert.hh"
#include "utils/proxy-server.hh"
#include "utils/test-patterns/test.hh"
#include "utils/test-suite.hh"
#include "utils/variant-utils.hh"

namespace {
using namespace flexisip;
using namespace flexisip::tester;
using namespace std;

shared_ptr<Server> makeAndStartProxy() {
	const auto proxy = make_shared<Server>(map<string, string>{
	    // Requesting bind on port 0 to let the kernel find any available port
	    {"global/transports", "sip:127.0.0.1:0;transport=tcp"},
	    {"module::Registrar/enabled", "true"},
	    {"module::Registrar/reg-domains", "sip.example.org"},
	    {"module::MediaRelay/enabled", "true"},
	    {"module::MediaRelay/prevent-loops", "false"}, // Allow loopback to localnetwork
	});
	proxy->start();
	return proxy;
}

template <typename... Callbacks>
void plugEventCallbacks(Agent& agent, overloaded<Callbacks...>&& callbacks) {
	agent.setEventLogWriter(unique_ptr<EventLogWriter>(new EventLogWriterVisitorAdapter{overloaded{
	    forward<overloaded<Callbacks...>>(callbacks),
	    [](const auto& log) {
		    ostringstream msg{};
		    msg << "This test is not supposed to write a " << typeid(log).name();
		    BC_HARD_FAIL(msg.str().c_str());
	    },
	}}));
}

string toString(const sip_from_t* from) {
	return eventlogs::sipDataToString(from->a_url);
}

template <typename Event>
auto moveEventsInto(vector<Event>& container) {
	return [&container](Event&& event) { container.emplace_back(move(event)); };
}

template <typename Event>
class Ignore {
public:
	void operator()(const Event&) {
	}
	void operator()(const shared_ptr<const Event>&) {
	}
};

string uuidOf(const linphone::Core& core) {
	return core.getConfig()->getString("misc", "uuid", "UNSET!");
}

string_view uuidFromSipInstance(const string_view& deviceKey) {
	return deviceKey.substr(sizeof("\"<urn:uuid:") - 1, sizeof("00000000-0000-0000-0000-000000000000") - 1);
}

void callStartedAndEnded() {
	const auto proxy = makeAndStartProxy();
	const auto& agent = proxy->getAgent();
	vector<CallStartedEventLog> callsStarted{};
	vector<CallRingingEventLog> callsRung{};
	vector<shared_ptr<const CallLog>> invitesEnded{};
	vector<CallEndedEventLog> callsEnded{};
	plugEventCallbacks(*agent, overloaded{
	                               moveEventsInto(callsStarted),
	                               moveEventsInto(invitesEnded),
	                               moveEventsInto(callsRung),
	                               moveEventsInto(callsEnded),
	                               Ignore<RegistrationLog>(),
	                           });
	const string expectedFrom = "sip:tony@sip.example.org";
	const string expectedTo = "sip:mike@sip.example.org";
	auto tony = ClientBuilder(expectedFrom).registerTo(proxy);
	auto mike = ClientBuilder(expectedTo).registerTo(proxy);
	const auto before = chrono::system_clock::now();

	tony.call(mike);

	BC_ASSERT_CPP_EQUAL(callsStarted.size(), 1);
	BC_ASSERT_CPP_EQUAL(callsRung.size(), 1);
	BC_ASSERT_CPP_EQUAL(invitesEnded.size(), 1);
	BC_ASSERT_CPP_EQUAL(callsEnded.size(), 0);
	const auto& startedEvent = callsStarted[0];
	BC_ASSERT_TRUE(before < startedEvent.mTimestamp);
	BC_ASSERT_CPP_EQUAL(toString(startedEvent.mFrom), expectedFrom);
	BC_ASSERT_CPP_EQUAL(toString(startedEvent.mTo), expectedTo);
	BC_ASSERT_CPP_EQUAL(startedEvent.mDevices.size(), 1);
	const string_view deviceKey = startedEvent.mDevices[0].mKey.str();
	BC_ASSERT_CPP_EQUAL(uuidFromSipInstance(deviceKey), uuidOf(*mike.getCore()));
	const string eventId = startedEvent.mId;
	const auto& ringingEvent = callsRung[0];
	BC_ASSERT_CPP_EQUAL(string(ringingEvent.mId), eventId);
	BC_ASSERT_CPP_EQUAL(ringingEvent.mDevice.mKey.str(), deviceKey);
	BC_ASSERT_TRUE(startedEvent.mTimestamp < ringingEvent.mTimestamp);
	const auto& acceptedEvent = invitesEnded[0];
	BC_ASSERT_CPP_EQUAL(toString(acceptedEvent->mFrom), expectedFrom);
	BC_ASSERT_CPP_EQUAL(toString(acceptedEvent->mTo), expectedTo);
	BC_ASSERT_CPP_EQUAL(string(acceptedEvent->mId), eventId);
	BC_ASSERT_TRUE(acceptedEvent->mDevice != nullopt);
	BC_ASSERT_CPP_EQUAL(acceptedEvent->mDevice->mKey.str(), deviceKey);
	const auto& acceptedAt = acceptedEvent->getDate();
	BC_ASSERT_TRUE(chrono::system_clock::to_time_t(ringingEvent.mTimestamp) <=
	               acceptedAt
	                   // Precision? Different clocks? I don't know why, but without this +1 it sometimes fails
	                   + 1);
	BC_ASSERT_CPP_EQUAL(acceptedEvent->getStatusCode(), 200 /* Accepted */);

	tony.endCurrentCall(mike);

	BC_ASSERT_CPP_EQUAL(callsEnded.size(), 1);
	const auto& endedEvent = callsEnded[0];
	BC_ASSERT_CPP_EQUAL(string(endedEvent.mId), eventId);
	BC_ASSERT_TRUE(acceptedAt <= chrono::system_clock::to_time_t(endedEvent.mTimestamp));
}

void callInviteStatuses() {
	const auto proxy = makeAndStartProxy();
	const auto& agent = proxy->getAgent();
	vector<shared_ptr<const CallLog>> invitesEnded{};
	plugEventCallbacks(
	    *agent, overloaded{
	                moveEventsInto(invitesEnded),
	                [&invitesEnded](CallLog&& owned) { invitesEnded.emplace_back(make_shared<CallLog>(move(owned))); },
	                Ignore<CallStartedEventLog>(),
	                Ignore<CallRingingEventLog>(),
	                Ignore<CallEndedEventLog>(),
	                Ignore<RegistrationLog>(),
	            });
	const string mike = "sip:mike@sip.example.org";
	auto tony = ClientBuilder("sip:tony@sip.example.org").registerTo(proxy);
	auto mikePhone = ClientBuilder(mike).registerTo(proxy);
	auto mikeDesktop = ClientBuilder(mike).registerTo(proxy);
	auto tonyCore = tony.getCore();
	auto mikePhoneCore = mikePhone.getCore();
	auto mikeDesktopCore = mikeDesktop.getCore();
	CoreAssert asserter{{tonyCore, mikePhoneCore, mikeDesktopCore}, agent};

	{
		auto tonyCall = tonyCore->invite(mike);
		mikePhone.hasReceivedCallFrom(tony).assert_passed();
		mikeDesktop.hasReceivedCallFrom(tony).assert_passed();
		tonyCall->terminate();
		asserter
		    .iterateUpTo(
		        4,
		        [mikePhoneCall = mikePhoneCore->getCurrentCall(), mikeDesktopCall = mikeDesktopCore->getCurrentCall()] {
			        FAIL_IF(mikePhoneCall->getState() != linphone::Call::State::End);
			        FAIL_IF(mikeDesktopCall->getState() != linphone::Call::State::End);
			        return ASSERTION_PASSED();
		        })
		    .assert_passed();
	}

	BC_ASSERT_CPP_EQUAL(invitesEnded.size(), 2);
	for (const auto& event : invitesEnded) {
		BC_ASSERT_CPP_EQUAL(event->isCancelled(), true);
		BC_ASSERT_ENUM_EQUAL(event->mForkStatus, ForkStatus::Standard);
	}
	invitesEnded.clear();

	{
		auto tonyCall = tonyCore->invite(mike);
		mikePhone.hasReceivedCallFrom(tony).assert_passed();
		mikeDesktop.hasReceivedCallFrom(tony).assert_passed();
		mikePhoneCore->getCurrentCall()->decline(linphone::Reason::Declined);
		asserter
		    .iterateUpTo(4,
		                 [&tonyCall, mikeDesktopCall = mikeDesktopCore->getCurrentCall()] {
			                 FAIL_IF(tonyCall->getState() != linphone::Call::State::End);
			                 FAIL_IF(mikeDesktopCall->getState() != linphone::Call::State::End);
			                 return ASSERTION_PASSED();
		                 })
		    .assert_passed();
	}

	BC_ASSERT_CPP_EQUAL(invitesEnded.size(), 2);
	const auto mikePhoneUuid = uuidOf(*mikePhoneCore);
	const auto mikeDesktopUuid = uuidOf(*mikeDesktopCore);
	unordered_map<string_view, reference_wrapper<const CallLog>> invitesByDeviceUuid{};
	for (const auto& event : invitesEnded) {
		BC_ASSERT_TRUE(event->mDevice != nullopt);
		invitesByDeviceUuid.emplace(uuidFromSipInstance(event->mDevice->mKey.str()), *event);
	}
	{
		const auto mikePhoneInvite = invitesByDeviceUuid.find(mikePhoneUuid);
		BC_ASSERT_TRUE(mikePhoneInvite != invitesByDeviceUuid.end());
		const auto& mikePhoneInviteEvent = mikePhoneInvite->second.get();
		BC_ASSERT_CPP_EQUAL(mikePhoneInviteEvent.isCancelled(), false);
		BC_ASSERT_CPP_EQUAL(mikePhoneInviteEvent.getStatusCode(), 603 /* Declined */);
		const auto mikeDesktopInvite = invitesByDeviceUuid.find(mikeDesktopUuid);
		BC_ASSERT_TRUE(mikeDesktopInvite != invitesByDeviceUuid.end());
		const auto& mikeDesktopInviteEvent = mikeDesktopInvite->second.get();
		BC_ASSERT_CPP_EQUAL(mikeDesktopInviteEvent.isCancelled(), true);
		BC_ASSERT_ENUM_EQUAL(mikeDesktopInviteEvent.mForkStatus, ForkStatus::DeclineElsewhere);
		invitesEnded.clear();
	}

	{
		auto tonyCall = tonyCore->invite(mike);
		mikePhone.hasReceivedCallFrom(tony).assert_passed();
		mikeDesktop.hasReceivedCallFrom(tony).assert_passed();
		mikePhoneCore->getCurrentCall()->accept();
		asserter
		    .iterateUpTo(4,
		                 [&tonyCall, mikeDesktopCall = mikeDesktopCore->getCurrentCall()] {
			                 FAIL_IF(tonyCall->getState() != linphone::Call::State::StreamsRunning);
			                 FAIL_IF(mikeDesktopCall->getState() != linphone::Call::State::End);
			                 return ASSERTION_PASSED();
		                 })
		    .assert_passed();
	}

	BC_ASSERT_CPP_EQUAL(invitesEnded.size(), 2);
	invitesByDeviceUuid.clear();
	for (const auto& event : invitesEnded) {
		BC_ASSERT_TRUE(event->mDevice != nullopt);
		invitesByDeviceUuid.emplace(uuidFromSipInstance(event->mDevice->mKey.str()), *event);
	}
	const auto mikePhoneInvite = invitesByDeviceUuid.find(mikePhoneUuid);
	BC_ASSERT_TRUE(mikePhoneInvite != invitesByDeviceUuid.end());
	const auto& mikePhoneInviteEvent = mikePhoneInvite->second.get();
	BC_ASSERT_CPP_EQUAL(mikePhoneInviteEvent.isCancelled(), false);
	BC_ASSERT_CPP_EQUAL(mikePhoneInviteEvent.getStatusCode(), 200 /* Accepted */);
	const auto mikeDesktopInvite = invitesByDeviceUuid.find(mikeDesktopUuid);
	BC_ASSERT_TRUE(mikeDesktopInvite != invitesByDeviceUuid.end());
	const auto& mikeDesktopInviteEvent = mikeDesktopInvite->second.get();
	BC_ASSERT_CPP_EQUAL(mikeDesktopInviteEvent.isCancelled(), true);
	BC_ASSERT_ENUM_EQUAL(mikeDesktopInviteEvent.mForkStatus, ForkStatus::AcceptedElsewhere);
}

void callError() {
	const auto proxy = makeAndStartProxy();
	const auto& agent = proxy->getAgent();
	vector<shared_ptr<const CallLog>> invitesEnded{};
	plugEventCallbacks(*agent, overloaded{
	                               moveEventsInto(invitesEnded),
	                               Ignore<CallStartedEventLog>(),
	                               Ignore<CallRingingEventLog>(),
	                               Ignore<RegistrationLog>(),
	                           });
	const string tradeFederation = "sip:TheTradeFederation@sip.example.org";
	auto galacticRepublicClient = ClientBuilder("sip:TheGalacticRepublic@sip.example.org").registerTo(proxy);
	auto tradeFederationClient = ClientBuilder(tradeFederation).registerTo(proxy);
	const auto republicCore = galacticRepublicClient.getCore();
	const auto federationCore = tradeFederationClient.getCore();
	CoreAssert asserter{{republicCore, federationCore}, agent};
	// The Republic and the Federation won't be able to negotiate a set of compatible params
	republicCore->setMediaEncryption(linphone::MediaEncryption::None);
	republicCore->setMediaEncryptionMandatory(false);
	federationCore->setMediaEncryption(linphone::MediaEncryption::SRTP);
	federationCore->setMediaEncryptionMandatory(true);

	republicCore->invite(tradeFederation);
	// "You were right about one thing, Master..."
	asserter.iterateUpTo(4, [&invitesEnded] {
		FAIL_IF(invitesEnded.empty());
		return ASSERTION_PASSED();
	});

	BC_ASSERT_CPP_EQUAL(invitesEnded.size(), 1);
	const auto& errorEvent = invitesEnded[0];
	BC_ASSERT_CPP_EQUAL(errorEvent->getStatusCode(), 488 /* Not acceptable */);
	BC_ASSERT_CPP_EQUAL(errorEvent->isCancelled(), false);
}

TestSuite _("EventLog Stats",
            {
                CLASSY_TEST(callStartedAndEnded),
                CLASSY_TEST(callInviteStatuses),
                CLASSY_TEST(callError),
            });
} // namespace
