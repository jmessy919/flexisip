/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "eventlogs/call-ringing-event-log.hh"

#include "eventlogs/event-log-variant.hh"
#include "eventlogs/identified.hh"
#include "eventlogs/type-complete-event-log-variant.hh"
#include "fork-context/branch-info.hh"

namespace flexisip {
using namespace std;

CallRingingEventLog::CallRingingEventLog(const sip_t& sip, const BranchInfo* branch)
    : Identified(sip), mDevice(*branch->mContact) {
}

eventlogs::Variant::Owned CallRingingEventLog::intoVariant() && {
	return move(*this);
}

eventlogs::Variant::Ref CallRingingEventLog::toRefVariant() const {
	return *this;
}

} // namespace flexisip
