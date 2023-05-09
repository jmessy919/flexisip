/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <chrono>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "sofia-sip/sip.h"

#include "eventlogs/event-id.hh"
#include "eventlogs/event-log-write-dispatcher.hh"
#include "eventlogs/sip-event-log.hh"

namespace flexisip {

using Timestamp = std::chrono::time_point<std::chrono::system_clock>;

class BranchInfo;
struct ExtendedContact;

class CallStartedEventLog : public EventLogWriteDispatcher, public SipEventLog {
public:
	CallStartedEventLog(const sip_t&, const std::list<std::shared_ptr<BranchInfo>>&);

	const EventId mId;
	const std::vector<ExtendedContact> mDevices;
	const Timestamp mInitiatedAt = std::chrono::system_clock::now();

protected:
	void write(EventLogWriter& writer) const override;
};

} // namespace flexisip
