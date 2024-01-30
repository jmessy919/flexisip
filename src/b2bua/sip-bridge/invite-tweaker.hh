/** Copyright (C) 2010-2024 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <linphone++/account.hh>
#include <linphone++/call.hh>

#include "accounts/account.hh"
#include "b2bua/sip-bridge/configuration/v2/v2.hh"
#include "utils/string-formatter.hh"

namespace flexisip::b2bua::bridge {

class InviteTweaker {
public:
	explicit InviteTweaker(const config::v2::OutgoingInvite&, linphone::Core&);

	std::shared_ptr<linphone::Address> tweakInvite(const linphone::Call&, const Account&, linphone::CallParams&) const;

private:
	/// The address to send the INVITE to
	StringFormatter mToHeader;
	std::optional<StringFormatter> mFromHeader;
	std::shared_ptr<linphone::Address> mOutboundProxyOverride;
	std::optional<bool> mAvpfOverride;
	std::optional<linphone::MediaEncryption> mEncryptionOverride;
};

} // namespace flexisip::b2bua::bridge
