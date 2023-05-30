/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "eventlogs/identified.hh"
#include "sofia-sip/sip.h"

#include "eventlogs/event-log-variant.hh"
#include "eventlogs/sip-event-log.hh"
#include "eventlogs/timestamped.hh"

namespace flexisip {

class BranchInfo;
struct ExtendedContact;

class CallStartedEventLog : public eventlogs::IntoEventLogVariant,
                            public SipEventLog,
                            public Identified,
                            public Timestamped {
public:
	CallStartedEventLog(const sip_t&, const std::list<std::shared_ptr<BranchInfo>>&);

	const std::vector<ExtendedContact> mDevices;

	eventlogs::EventLogVariant intoVariant() && override;
};

} // namespace flexisip
