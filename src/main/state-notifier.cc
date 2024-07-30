/*
    Flexisip, a flexible SIP proxy server with media capabilities.
    Copyright (C) 2010-2024 Belledonne Communications SARL, All rights reserved.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <cstring>
#include <unistd.h>

#include <flexisip/logmanager.hh>

#include "exceptions/exit.hh"
#include "state-notifier.hh"

using namespace std;

namespace flexisip {
StateNotifier::StateNotifier() {
	int err = pipe(mPipe);
	if (err == -1) {
		throw Exit{EXIT_FAILURE, "could not create pipes: "s + strerror(errno)};
	}
}
StateNotifier::StateNotifier(int flags) {
	int err = pipe2(mPipe, flags);
	if (err == -1) {
		throw Exit{EXIT_FAILURE, "could not create pipes: "s + strerror(errno)};
	}
}

StateNotifier::~StateNotifier() {
	close(mPipe[0]);
	close(mPipe[1]);
}

void StateNotifier::notify() {
	if (!mNotified) {
		if (write(mPipe[1], "ok", 3) == -1) {
			LOGF("Failed to write starter pipe: %s", strerror(errno));
		}
		mNotified = true;
	}
}

ssize_t StateNotifier::read(void* buf, size_t nbBytes) {
	if (nbBytes == 0) {
		nbBytes = sizeof(buf);
	}

	return ::read(mPipe[0], buf, nbBytes);
}
} // namespace flexisip