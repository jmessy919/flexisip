/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <memory>

#include "event-log-writer.hh"
#include "flexiapi/flexi-stats.hh"

namespace flexisip {

class FlexiStatsEventLogWriter : public EventLogWriter {
public:
	FlexiStatsEventLogWriter(sofiasip::SuRoot&,
	                         const std::string& host,
	                         const std::string& port,
	                         const std::string& token);

private:
#define IMPL(T) void write(const T&) override;

	IMPL(CallStartedEventLog)
	IMPL(CallRingingEventLog)
	IMPL(CallLog)
	IMPL(CallEndedEventLog)
#undef IMPL

#define STUB(T)                                                                                                        \
	void write(const T&) override {                                                                                    \
		SLOGD << "Stubbed: " << __PRETTY_FUNCTION__ << " is not implemented";                                          \
	}

	STUB(RegistrationLog)
	STUB(CallQualityStatisticsLog)
	STUB(MessageLog)
	STUB(AuthLog)

#undef STUB
	flexiapi::FlexiStats mRestClient;
};

} // namespace flexisip
