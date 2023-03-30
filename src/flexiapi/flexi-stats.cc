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

#include "flexi-stats.hh"

using namespace std;
using namespace flexisip;
using namespace nlohmann;

FlexiStats::FlexiStats(sofiasip::SuRoot& root,
                       const std::string& host,
                       const std::string& port,
                       const std::string& token)
    : mRestClient(Http2Client::make(root, host, port),
                  HttpHeaders{
                      {":authority"s, host + ":" + port},
                      {"x-api-key"s, token},
                  }) {
}

void FlexiStats::addMessage(const Message& message) {
	mRestClient.post("/api/stats/messages"s, message,
	                 string{"FlexiStats::addMessage request successful for id["s + message.id + "]"},
	                 string{"FlexiStats::addMessage request error for id["s + message.id + "]"});
}
// TODO URL ENCODE ?
void FlexiStats::notifyMessageDeviceResponse(const string& messageId,
                                             const string& sipUri,
                                             const std::string deviceId,
                                             const MessageDeviceResponse& messageDeviceResponse) {
	mRestClient.patch("/api/stats/messages/"s + messageId + "/to/" + sipUri + "/devices/" + deviceId,
	                  messageDeviceResponse,
	                  string{"FlexiStats::notifyMessageDeviceResponse request successful for id["s + messageId + "]"},
	                  string{"FlexiStats::notifyMessageDeviceResponse request error for id["s + messageId + "]"});
}

void FlexiStats::addCall(const Call& call) {
	mRestClient.post("/api/stats/calls"s, call,
	                 string{"FlexiStats::addCall request successful for id["s + call.id + "]"},
	                 string{"FlexiStats::addCall request error for id["s + call.id + "]"});
}
// TODO URL ENCODE ?
void FlexiStats::updateCallDeviceState(const string& callId,
                                       const string& deviceId,
                                       const CallDeviceState& callDeviceState) {
	mRestClient.patch("/api/stats/calls/"s + callId + "/devices/" + deviceId, callDeviceState,
	                  string{"FlexiStats::updateCallDeviceState request successful for id["s + callId + "]"},
	                  string{"FlexiStats::updateCallDeviceState request error for id["s + callId + "]"});
}
// TODO URL ENCODE ?
void FlexiStats::updateCallState(const string& callId, const string& endedAt) {
	mRestClient.patch("/api/stats/calls/"s + callId, optional<json>{json{{"ended_at", endedAt}}},
	                  string{"FlexiStats::updateCallState request successful for id["s + callId + "]"},
	                  string{"FlexiStats::updateCallState request error for id["s + callId + "]"});
}

void FlexiStats::addConference(const Conference& conference) {
	mRestClient.post("/api/stats/conferences"s, conference,
	                 string{"FlexiStats::addConference request successful for id["s + conference.id + "]"},
	                 string{"FlexiStats::addConference request error for id["s + conference.id + "]"});
}
// TODO URL ENCODE ?
void FlexiStats::notifyConferenceEnded(const string& conferenceId, const string& endedAt) {
	mRestClient.patch("/api/stats/conferences/"s + conferenceId, optional<json>{json{{"ended_at", endedAt}}},
	                  string{"FlexiStats::notifyConferenceEnded request successful for id["s + conferenceId + "]"},
	                  string{"FlexiStats::notifyConferenceEnded request error for id["s + conferenceId + "]"});
}
// TODO URL ENCODE ?
void FlexiStats::conferenceAddParticipantEvent(const string& conferenceId,
                                               const string& sipUri,
                                               const ParticipantEvent& participantEvent) {
	mRestClient.post(
	    "/api/stats/conferences/"s + conferenceId + "/participants/" + sipUri + "/events", participantEvent,
	    string{"FlexiStats::conferenceAddParticipantEvent request successful for id["s + conferenceId + "]"},
	    string{"FlexiStats::conferenceAddParticipantEvent request error for id["s + conferenceId + "]"});
}
void FlexiStats::conferenceAddParticipantDeviceEvent(const string& conferenceId,
                                                     const string& sipUri,
                                                     const string& deviceId,
                                                     const ParticipantDeviceEvent& participantDeviceEvent) {
	mRestClient.post(
	    "/api/stats/conferences/"s + conferenceId + "/participants/" + sipUri + "/devices/" + deviceId + "/events",
	    participantDeviceEvent,
	    string{"FlexiStats::conferenceAddParticipantDeviceEvent request successful for id["s + conferenceId + "]"},
	    string{"FlexiStats::conferenceAddParticipantDeviceEvent request error for id["s + conferenceId + "]"});
}
