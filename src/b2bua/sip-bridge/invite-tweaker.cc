/** Copyright (C) 2010-2024 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "invite-tweaker.hh"

#include <linphone++/account_params.hh>
#include <linphone++/address.hh>
#include <linphone++/call_params.hh>

namespace flexisip::b2bua::bridge {

InviteTweaker::InviteTweaker(config::v2::OutgoingInvite&& config)
    : mAvpfOverride(config.enableAvpf), mEncryptionOverride(config.mediaEncryption) {
}

std::shared_ptr<linphone::Address> InviteTweaker::tweakInvite(const linphone::Call& incomingCall,
                                                              const linphone::Account& linphoneAccount,
                                                              linphone::CallParams& outgoingCallParams) const {
	const auto callee = incomingCall.getRequestAddress()->clone();
	callee->setDomain(linphoneAccount.getParams()->getIdentityAddress()->getDomain());
	if (const auto& mediaEncryption = mEncryptionOverride) {
		outgoingCallParams.setMediaEncryption(*mediaEncryption);
	}
	if (const auto& enableAvpf = mAvpfOverride) {
		outgoingCallParams.enableAvpf(*enableAvpf);
	}

	return callee;
}

} // namespace flexisip::b2bua::bridge
