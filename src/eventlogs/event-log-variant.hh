/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <variant>

#include "call-ended-event-log.hh"
#include "call-ringing-event-log.hh"
#include "call-started-event-log.hh"
#include "eventlogs.hh"

namespace flexisip {

namespace eventlogs {

// template <typename... Events>
// struct EventLogTypes {
//	using Owned = std::variant<Events...>;
//	using Ref = std::variant<std::reference_wrapper<const Events>...>;
// };

using EventVariant = std::variant<RegistrationLog,
                                  CallStartedEventLog,
                                  CallRingingEventLog,
                                  CallLog,
                                  CallEndedEventLog,
                                  CallQualityStatisticsLog,
                                  MessageLog,
                                  AuthLog>;
// class IntoEventLogVariant {
// public:
//	virtual ~IntoEventLogVariant() = default;
//	virtual Variant::Owned intoVariant() && = 0;
// };
//
// class ToEventLogVariant {
// public:
//	virtual ~ToEventLogVariant() = default;
//	virtual Variant::Ref toRefVariant() const = 0;
// };

} // namespace eventlogs

} // namespace flexisip
