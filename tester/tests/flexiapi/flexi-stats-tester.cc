/*
    Flexisip, a flexible SIP proxy server with media capabilities.
    Copyright (C) 2010-2023 Belledonne Communications SARL, All rights reserved.

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

#include "tester.hh"

#include "flexiapi/flexi-stats.hh"
#include "lib/nlohmann-json-3-11-2/json.hpp"
#include "utils/http-mock/http-mock.hh"
#include "utils/test-patterns/test.hh"
#include "utils/test-suite.hh"

using namespace std;
using namespace std::chrono;
using namespace nlohmann;

namespace flexisip {
namespace tester {

// ####################################################################################################################
// ################################################### ABSTRACT TEST CLASS ############################################
// ####################################################################################################################

class FlexiStatsTest : public Test {
public:
	void operator()() override {
		HttpMock httpMock{{"/"}, &mRequestReceived};
		BC_HARD_ASSERT_TRUE(httpMock.serveAsync());

		FlexiStats flexiStats{mRoot, "localhost", "3000", "aRandomApiToken"};

		sendRequest(flexiStats);

		auto beforePlus2 = system_clock::now() + 2s;
		while (mRequestReceived != 1 && beforePlus2 >= system_clock::now()) {
			mRoot.step(10ms);
		}
		httpMock.forceCloseServer();
		mRoot.step(10ms); // needed to acknowledge mock server closing

		BC_HARD_ASSERT_TRUE(mRequestReceived);
		const auto actualRequest = httpMock.popRequestReceived();
		BC_HARD_ASSERT(actualRequest != nullptr);

		customAssert(actualRequest);
		BC_ASSERT_CPP_EQUAL(actualRequest->headers.size(), 1);
		BC_HARD_ASSERT_CPP_EQUAL(actualRequest->headers.count("x-api-key"), 1);
		BC_HARD_ASSERT_CPP_EQUAL(actualRequest->headers.find("x-api-key")->second.value, "aRandomApiToken");
	}

protected:
	virtual void sendRequest(FlexiStats& flexiStats) = 0;
	virtual void customAssert(const shared_ptr<Request>& actualRequest) = 0;

	std::atomic_int mRequestReceived = 0;

private:
	sofiasip::SuRoot mRoot{};
};

// ####################################################################################################################
// ################################################### ACTUAL TESTS ###################################################
// ####################################################################################################################

class AddMessageFullTest : public FlexiStatsTest {
protected:
	void sendRequest(FlexiStats& flexiStats) override {
		To to{
		    {"user1@domain.org",
		     MessageDevices{
		         {"device_id_1", MessageDeviceResponse{200, "2017-07-21T17:32:28Z"}},
		         {"device_id_2", MessageDeviceResponse{408, "2017-07-21T17:32:28Z"}},
		         {"device_id_3", nullopt},
		     }},
		    {"user2@domain.org",
		     MessageDevices{
		         {"device_id_1", MessageDeviceResponse{503, "2017-07-21T17:32:28Z"}},
		         {"device_id_2", nullopt},
		     }},
		};

		Message message{"84c937d1-f1b5-475d-adb7-b41b78b078d4",
		                "user@sip.linphone.org",
		                to,
		                "2017-07-21T17:32:28Z",
		                true,
		                "iHVDMq6MxSKp60bT"};

		flexiStats.addMessage(message);
	}

	void customAssert(const shared_ptr<Request>& actualRequest) override {
		BC_ASSERT_CPP_EQUAL(actualRequest->method, "POST");
		BC_ASSERT_CPP_EQUAL(actualRequest->path, "/api/stats/messages");
		json actualJson;
		try {
			actualJson = json::parse(actualRequest->body);
		} catch (const exception&) {
			BC_FAIL("json::parse exception with received body");
		}
		auto expectedJson = R"(
		{
		  "id": "84c937d1-f1b5-475d-adb7-b41b78b078d4",
		  "from": "user@sip.linphone.org",
		  "to": {
			"user1@domain.org": {
			  "device_id_1": {
				"last_status": 200,
				"received_at": "2017-07-21T17:32:28Z"
			  },
			  "device_id_2": {
				"last_status": 408,
				"received_at": "2017-07-21T17:32:28Z"
			  },
			  "device_id_3": null
			},
			"user2@domain.org": {
			  "device_id_1": {
				"last_status": 503,
				"received_at": "2017-07-21T17:32:28Z"
			  },
			  "device_id_2": null
			}
		  },
		  "sent_at": "2017-07-21T17:32:28Z",
		  "encrypted": true,
		  "conference_id": "iHVDMq6MxSKp60bT"
		}
		)"_json;
		BC_ASSERT_CPP_EQUAL(actualJson, expectedJson);
	}
};

class AddMessageMinimalTest : public FlexiStatsTest {
protected:
	void sendRequest(FlexiStats& flexiStats) override {
		Message message{"84c937d1-f1b5-475d-adb7-b41b78b078d4",
		                "user@sip.linphone.org",
		                To{},
		                "2017-07-21T17:32:28Z",
		                false,
		                nullopt};

		flexiStats.addMessage(message);
	}

	void customAssert(const shared_ptr<Request>& actualRequest) override {
		BC_ASSERT_CPP_EQUAL(actualRequest->method, "POST");
		BC_ASSERT_CPP_EQUAL(actualRequest->path, "/api/stats/messages");
		json actualJson;
		try {
			actualJson = json::parse(actualRequest->body);
		} catch (const exception&) {
			BC_FAIL("json::parse exception with received body");
		}
		auto expectedJson = R"(
		{
		  "id": "84c937d1-f1b5-475d-adb7-b41b78b078d4",
		  "from": "user@sip.linphone.org",
		  "to": {},
		  "sent_at": "2017-07-21T17:32:28Z",
		  "encrypted": false,
		  "conference_id": null
		}
		)"_json;
		BC_ASSERT_CPP_EQUAL(actualJson, expectedJson);
	}
};

class NotifyMessageDeviceResponseTest : public FlexiStatsTest {
protected:
	void sendRequest(FlexiStats& flexiStats) override {
		MessageDeviceResponse messageDeviceResponse{200, "2017-07-21T17:32:28Z"};

		flexiStats.notifyMessageDeviceResponse("84c937d1", "user1@domain.org", "device_id", messageDeviceResponse);
	}

	void customAssert(const shared_ptr<Request>& actualRequest) override {
		BC_ASSERT_CPP_EQUAL(actualRequest->method, "PATCH");
		BC_ASSERT_CPP_EQUAL(actualRequest->path, "/api/stats/messages/84c937d1/to/user1@domain.org/devices/device_id");
		json actualJson;
		try {
			actualJson = json::parse(actualRequest->body);
		} catch (const exception&) {
			BC_FAIL("json::parse exception with received body");
		}
		auto expectedJson = R"(
		{
		  "last_status": 200,
		  "received_at": "2017-07-21T17:32:28Z"
		}
		)"_json;
		BC_ASSERT_CPP_EQUAL(actualJson, expectedJson);
	}
};

class AddCallFullTest : public FlexiStatsTest {
protected:
	void sendRequest(FlexiStats& flexiStats) override {
		CallDevices callDevices{
		    {"device_id_1",
		     CallDeviceState{"2017-07-21T17:32:28Z", Terminated{"2017-07-21T18:32:28Z", TerminatedState::ACCEPTED}}},
		    {"device_id_2", CallDeviceState{"2017-07-21T17:32:28Z",
		                                    Terminated{"2017-07-21T18:32:28Z", TerminatedState::ACCEPTED_ELSEWHERE}}},
		    {"device_id_3", nullopt},
		};

		Call call{"4722b0233fd8cafad3cdcafe5510fe57",
		          "user@sip.linphone.org",
		          "user@sip.linphone.org",
		          callDevices,
		          "2017-07-21T17:32:28Z",
		          "2017-07-21T19:42:26Z",
		          "iHVDMq6MxSKp60bT"};

		flexiStats.addCall(call);
	}

	void customAssert(const shared_ptr<Request>& actualRequest) override {
		BC_ASSERT_CPP_EQUAL(actualRequest->method, "POST");
		BC_ASSERT_CPP_EQUAL(actualRequest->path, "/api/stats/calls");
		json actualJson;
		try {
			actualJson = json::parse(actualRequest->body);
		} catch (const exception&) {
			BC_FAIL("json::parse exception with received body");
		}
		auto expectedJson = R"(
		{
		  "id": "4722b0233fd8cafad3cdcafe5510fe57",
		  "from": "user@sip.linphone.org",
		  "to": "user@sip.linphone.org",
		  "devices": {
			"device_id_1": {
			  "rang_at": "2017-07-21T17:32:28Z",
			  "invite_terminated": {
				"at": "2017-07-21T18:32:28Z",
				"state": "accepted"
			  }
			},
			"device_id_2": {
			  "rang_at": "2017-07-21T17:32:28Z",
			  "invite_terminated": {
				"at": "2017-07-21T18:32:28Z",
				"state": "accepted_elsewhere"
			  }
			},
			"device_id_3": null
		  },
		  "initiated_at": "2017-07-21T17:32:28Z",
		  "ended_at": "2017-07-21T19:42:26Z",
		  "conference_id": "iHVDMq6MxSKp60bT"
		}
		)"_json;
		BC_ASSERT_CPP_EQUAL(actualJson, expectedJson);
	}
};

class AddCallMinimalTest : public FlexiStatsTest {
protected:
	void sendRequest(FlexiStats& flexiStats) override {
		Call call{"4722b0233fd8cafad3cdcafe5510fe57",
		          "user@sip.linphone.org",
		          "user@sip.linphone.org",
		          CallDevices{},
		          "2017-07-21T17:32:28Z",
		          nullopt,
		          nullopt};

		flexiStats.addCall(call);
	}

	void customAssert(const shared_ptr<Request>& actualRequest) override {
		BC_ASSERT_CPP_EQUAL(actualRequest->method, "POST");
		BC_ASSERT_CPP_EQUAL(actualRequest->path, "/api/stats/calls");
		json actualJson;
		try {
			actualJson = json::parse(actualRequest->body);
		} catch (const exception&) {
			BC_FAIL("json::parse exception with received body");
		}
		auto expectedJson = R"(
		{
		  "id": "4722b0233fd8cafad3cdcafe5510fe57",
		  "from": "user@sip.linphone.org",
		  "to": "user@sip.linphone.org",
		  "devices": {},
		  "initiated_at": "2017-07-21T17:32:28Z",
		  "ended_at": null,
		  "conference_id": null
		}
		)"_json;
		BC_ASSERT_CPP_EQUAL(actualJson, expectedJson);
	}
};

class UpdateCallDeviceStateFullTest : public FlexiStatsTest {
protected:
	void sendRequest(FlexiStats& flexiStats) override {
		CallDeviceState callDeviceState{"2017-07-21T17:32:28Z",
		                                Terminated{"2017-07-21T17:32:28Z", TerminatedState::ERROR}};

		flexiStats.updateCallDeviceState("4722b0233", "device_id", callDeviceState);
	}

	void customAssert(const shared_ptr<Request>& actualRequest) override {
		BC_ASSERT_CPP_EQUAL(actualRequest->method, "PATCH");
		BC_ASSERT_CPP_EQUAL(actualRequest->path, "/api/stats/calls/4722b0233/devices/device_id");
		json actualJson;
		try {
			actualJson = json::parse(actualRequest->body);
		} catch (const exception&) {
			BC_FAIL("json::parse exception with received body");
		}
		auto expectedJson = R"(
		{
		  "rang_at": "2017-07-21T17:32:28Z",
		  "invite_terminated": {
			"at": "2017-07-21T17:32:28Z",
			"state": "error"
		  }
		}
		)"_json;
		BC_ASSERT_CPP_EQUAL(actualJson, expectedJson);
	}
};

class UpdateCallDeviceStateRangOnlyTest : public FlexiStatsTest {
protected:
	void sendRequest(FlexiStats& flexiStats) override {
		CallDeviceState callDeviceState{"2017-07-21T17:32:28Z", nullopt};

		flexiStats.updateCallDeviceState("4722b0233", "device_id_1", callDeviceState);
	}

	void customAssert(const shared_ptr<Request>& actualRequest) override {
		BC_ASSERT_CPP_EQUAL(actualRequest->method, "PATCH");
		BC_ASSERT_CPP_EQUAL(actualRequest->path, "/api/stats/calls/4722b0233/devices/device_id_1");
		json actualJson;
		try {
			actualJson = json::parse(actualRequest->body);
		} catch (const exception&) {
			BC_FAIL("json::parse exception with received body");
		}
		auto expectedJson = R"(
		{
		  "rang_at": "2017-07-21T17:32:28Z"
		}
		)"_json;
		BC_ASSERT_CPP_EQUAL(actualJson, expectedJson);
	}
};

class UpdateCallDeviceStateTerminatedOnlyTest : public FlexiStatsTest {
protected:
	void sendRequest(FlexiStats& flexiStats) override {
		CallDeviceState callDeviceState{nullopt, Terminated{"2017-07-21T17:32:28Z", TerminatedState::DECLINED}};

		flexiStats.updateCallDeviceState("4722b0233", "device_id_1", callDeviceState);
	}

	void customAssert(const shared_ptr<Request>& actualRequest) override {
		BC_ASSERT_CPP_EQUAL(actualRequest->method, "PATCH");
		BC_ASSERT_CPP_EQUAL(actualRequest->path, "/api/stats/calls/4722b0233/devices/device_id_1");
		json actualJson;
		try {
			actualJson = json::parse(actualRequest->body);
		} catch (const exception&) {
			BC_FAIL("json::parse exception with received body");
		}
		auto expectedJson = R"(
		{
		  "invite_terminated": {
			"at": "2017-07-21T17:32:28Z",
			"state": "declined"
		  }
		}
		)"_json;
		BC_ASSERT_CPP_EQUAL(actualJson, expectedJson);
	}
};

class UpdateCallDeviceStateEmptyTest : public FlexiStatsTest {
protected:
	void sendRequest(FlexiStats& flexiStats) override {
		CallDeviceState callDeviceState{nullopt, nullopt};

		flexiStats.updateCallDeviceState("4722b0233", "device_id_1", callDeviceState);
	}

	void customAssert(const shared_ptr<Request>& actualRequest) override {
		BC_ASSERT_CPP_EQUAL(actualRequest->method, "PATCH");
		BC_ASSERT_CPP_EQUAL(actualRequest->path, "/api/stats/calls/4722b0233/devices/device_id_1");
		json actualJson;
		try {
			actualJson = json::parse(actualRequest->body);
		} catch (const exception&) {
			BC_FAIL("json::parse exception with received body");
		}
		auto expectedJson = R"({})"_json;
		BC_ASSERT_CPP_EQUAL(actualJson, expectedJson);
	}
};

class UpdateCallStateTest : public FlexiStatsTest {
protected:
	void sendRequest(FlexiStats& flexiStats) override {
		flexiStats.updateCallState("4722b0233", "2017-07-21T19:42:26Z");
	}

	void customAssert(const shared_ptr<Request>& actualRequest) override {
		BC_ASSERT_CPP_EQUAL(actualRequest->method, "PATCH");
		BC_ASSERT_CPP_EQUAL(actualRequest->path, "/api/stats/calls/4722b0233");
		json actualJson;
		try {
			actualJson = json::parse(actualRequest->body);
		} catch (const exception&) {
			BC_FAIL("json::parse exception with received body");
		}
		auto expectedJson = R"(
		{
			"ended_at": "2017-07-21T19:42:26Z"
		}
		)"_json;
		BC_ASSERT_CPP_EQUAL(actualJson, expectedJson);
	}
};

class AddConferenceFullTest : public FlexiStatsTest {
protected:
	void sendRequest(FlexiStats& flexiStats) override {
		Conference conference{"iHVDMq6MxSKp60bT", "2017-07-21T17:32:28Z", "2017-07-21T17:32:28Z", "string"};

		flexiStats.addConference(conference);
	}

	void customAssert(const shared_ptr<Request>& actualRequest) override {
		BC_ASSERT_CPP_EQUAL(actualRequest->method, "POST");
		BC_ASSERT_CPP_EQUAL(actualRequest->path, "/api/stats/conferences");
		json actualJson;
		try {
			actualJson = json::parse(actualRequest->body);
		} catch (const exception&) {
			BC_FAIL("json::parse exception with received body");
		}
		auto expectedJson = R"(
		{
			"id": "iHVDMq6MxSKp60bT",
			"created_at": "2017-07-21T17:32:28Z",
			"ended_at": "2017-07-21T17:32:28Z",
			"schedule": "string"
		}
		)"_json;
		BC_ASSERT_CPP_EQUAL(actualJson, expectedJson);
	}
};

class AddConferenceMinimalTest : public FlexiStatsTest {
protected:
	void sendRequest(FlexiStats& flexiStats) override {
		Conference conference{"iHVDMq6MxSKp60bT", "2017-07-21T17:32:28Z", nullopt, nullopt};

		flexiStats.addConference(conference);
	}

	void customAssert(const shared_ptr<Request>& actualRequest) override {
		BC_ASSERT_CPP_EQUAL(actualRequest->method, "POST");
		BC_ASSERT_CPP_EQUAL(actualRequest->path, "/api/stats/conferences");
		json actualJson;
		try {
			actualJson = json::parse(actualRequest->body);
		} catch (const exception&) {
			BC_FAIL("json::parse exception with received body");
		}
		auto expectedJson = R"(
		{
			"id": "iHVDMq6MxSKp60bT",
			"created_at": "2017-07-21T17:32:28Z",
			"ended_at": null,
			"schedule": null
		}
		)"_json;
		BC_ASSERT_CPP_EQUAL(actualJson, expectedJson);
	}
};

class NotifyConferenceEndedTest : public FlexiStatsTest {
protected:
	void sendRequest(FlexiStats& flexiStats) override {
		flexiStats.notifyConferenceEnded("iHVDMq6MxSKp60bT", "2017-07-21T17:32:28Z");
	}

	void customAssert(const shared_ptr<Request>& actualRequest) override {
		BC_ASSERT_CPP_EQUAL(actualRequest->method, "PATCH");
		BC_ASSERT_CPP_EQUAL(actualRequest->path, "/api/stats/conferences/iHVDMq6MxSKp60bT");
		json actualJson;
		try {
			actualJson = json::parse(actualRequest->body);
		} catch (const exception&) {
			BC_FAIL("json::parse exception with received body");
		}
		auto expectedJson = R"(
		{
			"ended_at": "2017-07-21T17:32:28Z"
		}
		)"_json;
		BC_ASSERT_CPP_EQUAL(actualJson, expectedJson);
	}
};

class ConferenceAddParticipantEventTest : public FlexiStatsTest {
protected:
	void sendRequest(FlexiStats& flexiStats) override {
		ParticipantEvent participantEvent{ParticipantEventType::ADDED, "2017-07-21T17:32:28Z"};
		flexiStats.conferenceAddParticipantEvent("iHVDMq6MxSKp60bT", "user1@domain.org", participantEvent);
	}

	void customAssert(const shared_ptr<Request>& actualRequest) override {
		BC_ASSERT_CPP_EQUAL(actualRequest->method, "POST");
		BC_ASSERT_CPP_EQUAL(actualRequest->path,
		                    "/api/stats/conferences/iHVDMq6MxSKp60bT/participants/user1@domain.org/events");
		json actualJson;
		try {
			actualJson = json::parse(actualRequest->body);
		} catch (const exception&) {
			BC_FAIL("json::parse exception with received body");
		}
		auto expectedJson = R"(
		{
			"type": "added",
			"at": "2017-07-21T17:32:28Z"
		}
		)"_json;
		BC_ASSERT_CPP_EQUAL(actualJson, expectedJson);
	}
};

class ConferenceAddParticipantDeviceEventTest : public FlexiStatsTest {
protected:
	void sendRequest(FlexiStats& flexiStats) override {
		ParticipantDeviceEvent participantDeviceEvent{ParticipantDeviceEventType::INVITED, "2017-07-21T17:32:28Z"};
		flexiStats.conferenceAddParticipantDeviceEvent("iHVDMq6MxSKp60bT", "user1@domain.org", "device_id",
		                                               participantDeviceEvent);
	}

	void customAssert(const shared_ptr<Request>& actualRequest) override {
		BC_ASSERT_CPP_EQUAL(actualRequest->method, "POST");
		BC_ASSERT_CPP_EQUAL(
		    actualRequest->path,
		    "/api/stats/conferences/iHVDMq6MxSKp60bT/participants/user1@domain.org/devices/device_id/events");
		json actualJson;
		try {
			actualJson = json::parse(actualRequest->body);
		} catch (const exception&) {
			BC_FAIL("json::parse exception with received body");
		}
		auto expectedJson = R"(
		{
			"type": "invited",
			"at": "2017-07-21T17:32:28Z"
		}
		)"_json;
		BC_ASSERT_CPP_EQUAL(actualJson, expectedJson);
	}
};

namespace {
TestSuite _("FlexiStats client unit tests",
            {
                CLASSY_TEST(AddMessageFullTest),
                CLASSY_TEST(AddMessageMinimalTest),
                CLASSY_TEST(NotifyMessageDeviceResponseTest),
                CLASSY_TEST(AddCallFullTest),
                CLASSY_TEST(AddCallMinimalTest),
                CLASSY_TEST(UpdateCallDeviceStateFullTest),
                CLASSY_TEST(UpdateCallDeviceStateRangOnlyTest),
                CLASSY_TEST(UpdateCallDeviceStateTerminatedOnlyTest),
                CLASSY_TEST(UpdateCallDeviceStateEmptyTest),
                CLASSY_TEST(UpdateCallStateTest),
                CLASSY_TEST(AddConferenceFullTest),
                CLASSY_TEST(AddConferenceMinimalTest),
                CLASSY_TEST(AddConferenceMinimalTest),
                CLASSY_TEST(NotifyConferenceEndedTest),
                CLASSY_TEST(ConferenceAddParticipantEventTest),
                CLASSY_TEST(ConferenceAddParticipantDeviceEventTest),
            });
} // namespace

} // namespace tester
} // namespace flexisip
