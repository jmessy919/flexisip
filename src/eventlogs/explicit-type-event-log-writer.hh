/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <memory>

#include "flexisip/logmanager.hh"

#include "eventlogs/event-log-writer.hh"

namespace flexisip {

class RegistrationLog;
class CallLog;
class CallQualityStatisticsLog;
class MessageLog;
class AuthLog;

class ExplicitTypeEventLogWriter : public EventLogWriter {
public:
	virtual ~ExplicitTypeEventLogWriter() = default;

	void write(eventlogs::EventVariant&& event) override;
	void write(const std::shared_ptr<const eventlogs::EventVariant>& sharedVariant) override;

protected:
	virtual void write(const RegistrationLog& rlog) = 0;
	virtual void write(const CallLog& clog) = 0;
	virtual void write(const CallQualityStatisticsLog& mlog) = 0;
	virtual void write(const MessageLog& mlog) = 0;
	virtual void write(const AuthLog& alog) = 0;

private:
	template <typename Event>
	void write(const Event&) {
		SLOGD << typeid(*this).name() << " does not implement " << __PRETTY_FUNCTION__;
	}
};

} // namespace flexisip
