/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <chrono>

#include "sofia-sip/sip.h"

#include "eventlogs/event-id.hh"
#include "eventlogs/event-log-write-dispatcher.hh"
#include "registrar/extended-contact.hh"

namespace flexisip {

using Timestamp = std::chrono::time_point<std::chrono::system_clock>;

class BranchInfo;

class CallRingingEventLog : public EventLogWriteDispatcher {
public:
	CallRingingEventLog(const sip_t&, const BranchInfo*);

	const EventId mId;
	const ExtendedContact mDevice;
	const Timestamp mRingingAt = std::chrono::system_clock::now();

protected:
	void write(EventLogWriter& writer) const override;
};

} // namespace flexisip
