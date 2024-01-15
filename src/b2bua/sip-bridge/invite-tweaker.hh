/** Copyright (C) 2010-2024 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <linphone++/account.hh>
#include <linphone++/call.hh>

#include "configuration/v2.hh"

namespace flexisip::b2bua::bridge {

class InviteTweaker {
public:
	explicit InviteTweaker(config::v2::OutgoingInvite&&);

	std::shared_ptr<linphone::Address>
	tweakInvite(const linphone::Call&, const linphone::Account&, linphone::CallParams&) const;

private:
	std::optional<bool> mAvpfOverride;
	std::optional<linphone::MediaEncryption> mEncryptionOverride;
};

} // namespace flexisip::b2bua::bridge
