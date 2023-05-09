/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <memory>

#include "eventlogs/event-log-write-dispatcher.hh"
#include "flexisip/logmanager.hh"

namespace flexisip {

class RegistrationLog;
class CallStartedEventLog;
class CallRingingEventLog;
class CallLog;
class CallQualityStatisticsLog;
class MessageLog;
class AuthLog;

class EventLogWriter {
public:
	EventLogWriter() = default;
	EventLogWriter(const EventLogWriter&) = delete;
	virtual ~EventLogWriter() = default;

	virtual void write(std::shared_ptr<const EventLogWriteDispatcher> evlog) {
		evlog->write(*this);
	}

protected:
	friend RegistrationLog;
	friend CallStartedEventLog;
	friend CallRingingEventLog;
	friend CallLog;
	friend CallQualityStatisticsLog;
	friend MessageLog;
	friend AuthLog;

	virtual void write(const RegistrationLog& rlog) = 0;
	virtual void write(const CallStartedEventLog&) {
		SLOGD << typeid(*this).name() << " does not implement " << __PRETTY_FUNCTION__;
	}
	virtual void write(const CallRingingEventLog&) {
		SLOGD << typeid(*this).name() << " does not implement " << __PRETTY_FUNCTION__;
	}
	virtual void write(const CallLog& clog) = 0;
	virtual void write(const CallQualityStatisticsLog& mlog) = 0;
	virtual void write(const MessageLog& mlog) = 0;
	virtual void write(const AuthLog& alog) = 0;
};

} // namespace flexisip
