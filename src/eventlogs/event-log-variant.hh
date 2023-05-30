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

using EventLogVariant = std::variant<RegistrationLog,
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
	virtual EventLogVariant intoVariant() && = 0;
};

using EventLogRefVariant = std::variant<std::reference_wrapper<const RegistrationLog>,
                                        std::reference_wrapper<const CallStartedEventLog>,
                                        std::reference_wrapper<const CallRingingEventLog>,
                                        std::reference_wrapper<const CallLog>,
                                        std::reference_wrapper<const CallEndedEventLog>,
                                        std::reference_wrapper<const CallQualityStatisticsLog>,
                                        std::reference_wrapper<const MessageLog>,
                                        std::reference_wrapper<const AuthLog>>;

class ToEventLogVariant {
public:
	virtual ~ToEventLogVariant() = default;
	virtual EventLogRefVariant toRefVariant() const = 0;
};

} // namespace eventlogs

} // namespace flexisip
