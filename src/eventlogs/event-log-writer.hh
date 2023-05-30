/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <memory>

namespace flexisip {
namespace eventlogs {

class IntoEventLogVariant;
class ToEventLogVariant;

} // namespace eventlogs

class EventLogWriter {
public:
	EventLogWriter() = default;
	EventLogWriter(const EventLogWriter&) = delete;
	virtual ~EventLogWriter() = default;

	virtual void write(eventlogs::IntoEventLogVariant&&) = 0;
	virtual void write(const std::shared_ptr<const eventlogs::ToEventLogVariant>&) = 0;
};

} // namespace flexisip
