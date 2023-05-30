/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <memory>
#include <type_traits>
#include <utility>
#include <variant>

#include "eventlogs/event-log-variant.hh"
#include "eventlogs/event-log-writer.hh"
#include "eventlogs/type-complete-event-log-variant.hh"

namespace flexisip {

template <typename TVisitor>
class EventLogWriterVisitorAdapter : public EventLogWriter {
public:
	EventLogWriterVisitorAdapter(TVisitor&& visitor) : mVisitor(std::move(visitor)) {
	}

	void write(eventlogs::IntoEventLogVariant&& event) override {
		std::visit(mVisitor, std::move(event).intoVariant());
	}
	void write(const std::shared_ptr<const eventlogs::ToEventLogVariant>& event) override {
		std::visit(
		    [this, &event](const auto& ref) {
			    using TEvent = typename std::decay_t<decltype(ref)>::type;
			    mVisitor(std::dynamic_pointer_cast<TEvent>(event));
		    },
		    event->toRefVariant());
	}

private:
	TVisitor mVisitor;
};

} // namespace flexisip
