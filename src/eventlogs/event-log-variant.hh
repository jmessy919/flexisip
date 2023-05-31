/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <variant>

namespace flexisip {

class RegistrationLog;
class CallStartedEventLog;
class CallRingingEventLog;
class CallLog;
class CallEndedEventLog;
class CallQualityStatisticsLog;
class MessageLog;
class AuthLog;

namespace eventlogs {

template <typename... Events>
struct EventLogTypes {
	using Owned = std::variant<Events...>;
	using Ref = std::variant<std::reference_wrapper<const Events>...>;
};

using Variant = EventLogTypes<RegistrationLog,
                              CallStartedEventLog,
                              CallRingingEventLog,
                              CallLog,
                              CallEndedEventLog,
                              CallQualityStatisticsLog,
                              MessageLog,
                              AuthLog>;
class IntoEventLogVariant {
public:
	virtual ~IntoEventLogVariant() = default;
	virtual Variant::Owned intoVariant() && = 0;
};

class ToEventLogVariant {
public:
	virtual ~ToEventLogVariant() = default;
	virtual Variant::Ref toRefVariant() const = 0;
};

} // namespace eventlogs

} // namespace flexisip
