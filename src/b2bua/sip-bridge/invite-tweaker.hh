/** Copyright (C) 2010-2024 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <linphone++/account.hh>
#include <linphone++/call.hh>

#include "accounts/account.hh"
#include "b2bua/sip-bridge/configuration/v2/v2.hh"
#include "utils/string-interpolation/preprocessed-interpolated-string.hh"

namespace flexisip::b2bua::bridge {

class InviteTweaker {
public:
	using StringTemplate =
	    utils::string_interpolation::PreprocessedInterpolatedString<const linphone::Call&, const Account&>;

	explicit InviteTweaker(const config::v2::OutgoingInvite&, linphone::Core&);

	std::shared_ptr<linphone::Address> tweakInvite(const linphone::Call&, const Account&, linphone::CallParams&) const;

private:
	/// The address to send the INVITE to
	StringTemplate mToHeader;
	std::optional<StringTemplate> mFromHeader;
	std::shared_ptr<linphone::Address> mOutboundProxyOverride;
	std::optional<bool> mAvpfOverride;
	std::optional<linphone::MediaEncryption> mEncryptionOverride;
};

} // namespace flexisip::b2bua::bridge
