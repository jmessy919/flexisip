/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <string>

#include <sofia-sip/sip_protos.h>

namespace flexisip {

namespace eventlogs {

std::string sipDataToString(const url_t* url);

}

} // namespace flexisip
