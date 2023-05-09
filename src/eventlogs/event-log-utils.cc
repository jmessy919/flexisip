/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "eventlogs/event-log-utils.hh"

#include <string>

#include <sofia-sip/sip_protos.h>

using namespace std;

namespace flexisip {

namespace eventlogs {

string sipDataToString(const url_t* url) {
	if (!url) {
		return string();
	}

	char tmp[256] = {};
	url_e(tmp, sizeof(tmp) - 1, url);
	return string(tmp);
}

}

} // namespace flexisip
