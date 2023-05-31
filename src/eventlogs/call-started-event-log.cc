/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "eventlogs/call-started-event-log.hh"

#include <type_traits>
#include <vector>

#include "eventlogs/identified.hh"
#include "eventlogs/type-complete-event-log-variant.hh"
#include "fork-context/branch-info.hh"
#include "registrar/extended-contact.hh"

namespace flexisip {
using namespace std;

CallStartedEventLog::CallStartedEventLog(const sip_t& sip, const std::list<std::shared_ptr<BranchInfo>>& branchInfoList)
    : SipEventLog(sip), Identified(sip)  {
		mDevices.reserve(branchInfoList.size());
	      for (const auto& branchInfo : branchInfoList) {
		      mDevices.emplace_back(*branchInfo->mContact);
	      }
}

eventlogs::EventLogVariant CallStartedEventLog::intoVariant() && {
	return move(*this);
}

} // namespace flexisip
