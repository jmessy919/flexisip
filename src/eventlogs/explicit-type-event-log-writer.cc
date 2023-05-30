/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "eventlogs/explicit-type-event-log-writer.hh"

#include <variant>

#include "eventlogs/type-complete-event-log-variant.hh"

using namespace std;

namespace flexisip {

void ExplicitTypeEventLogWriter::write(eventlogs::IntoEventLogVariant&& event) {
	std::visit([this](auto&& variant) { write(variant); }, move(event).intoVariant());
}
void ExplicitTypeEventLogWriter::write(const std::shared_ptr<const eventlogs::ToEventLogVariant>& event) {
	std::visit([this](const auto& variant) { write(variant.get()); }, event->toRefVariant());
}

} // namespace flexisip
