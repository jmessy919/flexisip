/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "sofia-sip/sip.h"

#include "eventlogs/identified.hh"
#include "eventlogs/timestamped.hh"
#include "registrar/extended-contact.hh"

namespace flexisip {

class BranchInfo;

class CallRingingEventLog : /*public eventlogs::IntoEventLogVariant,
                            public eventlogs::ToEventLogVariant,*/
                            public Identified,
                            public Timestamped {
public:
	CallRingingEventLog(const sip_t&, const BranchInfo*);

	const ExtendedContact mDevice;

	bool isCompleted() const {
		return true;
	}
	//	eventlogs::Variant::Owned intoVariant() && override;
	//	eventlogs::Variant::Ref toRefVariant() const override;
};

} // namespace flexisip
