/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <memory>
#include <utility>
#include <variant>

#include "eventlogs/event-log-writer.hh"

namespace flexisip {

template <typename TVisitor>
class EventLogWriterVisitorAdapter : public EventLogWriter {
public:
	EventLogWriterVisitorAdapter(TVisitor&& visitor) : mVisitor(std::move(visitor)) {
	}

protected:
	void write(const RegistrationLog& event) override {
		mVisitor(event);
	}
	void write(const CallStartedEventLog& event) override {
		mVisitor(event);
	}
	void write(const CallRingingEventLog& event) override {
		mVisitor(event);
	}
	void write(const CallLog& event) override {
		mVisitor(event);
	}
	void write(const CallQualityStatisticsLog& event) override {
		mVisitor(event);
	}
	void write(const MessageLog& event) override {
		mVisitor(event);
	}
	void write(const AuthLog& event) override {
		mVisitor(event);
	}

private:
	TVisitor mVisitor;
};

} // namespace flexisip
