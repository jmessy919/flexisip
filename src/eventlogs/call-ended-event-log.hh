/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "sofia-sip/sip.h"

#include "eventlogs/identified.hh"
#include "eventlogs/event-log-variant.hh"
#include "eventlogs/timestamped.hh"

namespace flexisip {

class BranchInfo;

class CallEndedEventLog : public eventlogs::IntoEventLogVariant, public Identified, public Timestamped {
public:
	CallEndedEventLog(const sip_t&);

	eventlogs::Variant::Owned intoVariant() && override;
};

} // namespace flexisip
