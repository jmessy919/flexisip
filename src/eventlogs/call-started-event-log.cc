/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "eventlogs/call-started-event-log.hh"

#include <type_traits>
#include <vector>

#include "eventlogs/event-log-writer.hh"
#include "eventlogs/identified.hh"
#include "fork-context/branch-info.hh"
#include "registrar/extended-contact.hh"

namespace flexisip {
using namespace std;

CallStartedEventLog::CallStartedEventLog(const sip_t& sip, const std::list<std::shared_ptr<BranchInfo>>& branchInfoList)
    : SipEventLog(sip), Identified(sip), mDevices([&branchInfoList] {
	      std::remove_const_t<decltype(mDevices)> devices{};
	      devices.reserve(branchInfoList.size());
	      for (const auto& branchInfo : branchInfoList) {
		      devices.emplace_back(*branchInfo->mContact);
	      }
	      return devices;
      }()) {
}

void CallStartedEventLog::write(EventLogWriter& writer) const {
	writer.write(*this);
}

} // namespace flexisip
