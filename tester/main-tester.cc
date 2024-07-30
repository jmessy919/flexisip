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

#include "utils/asserts.hh"
#include "utils/test-patterns/test.hh"
#include "utils/test-suite.hh"

#include <csignal>
#include <filesystem>
#include <future>
#include <iostream>
#include <sys/wait.h>

#include "main/flexisip.hh"
#include "main/state-notifier.hh"
#include "tester.hh"

using namespace std;

namespace flexisip::tester {

/**
 * Test the main function of flexisip server
 *
 * Start a flexisip server with all service types activated then check that all service are properly initialized.
 * Stop the server and check that the program exit cleanly.
 *
 */
void callAndStopMain() {
	//	auto confFilePath = std::filesystem::path(bc_tester_get_writable_dir_prefix()) / "flexisip.conf";
	auto confFilePath{bcTesterRes("config/flexisip-main-all-services.conf")};
	vector<const char*> args{
	    "flexisip",
	    "-c",
	    confFilePath.c_str(),
	    "--server all",
	};

	StateNotifier startNotifier{O_NONBLOCK};
	auto childPid = fork();
	if (childPid == 0) {
		// Child process: Execute main and exit
		::exit(_main(args.size(), const_cast<char**>(&args[0]), ::move(startNotifier)));
	}
	// Main process:
	// Check that flexisip started, stop it and check that it exited cleanly
	BcAssert asserter{};
	uint8_t buf[4];
	ssize_t count;
	asserter
	    .iterateUpTo(
	        10,
	        [&startNotifier, &buf, &count]() {
		        count = startNotifier.read(buf);
		        return LOOP_ASSERTION(count > 0);
	        },
	        2s)
	    .hard_assert_passed();

	// Short wait to ensure that main loop starts
	sleep(1);
	// Stop flexisip execution
	BC_HARD_ASSERT_CPP_EQUAL(::kill(childPid, SIGINT), 0);

	// Ensure clean exit from flexisip
	asserter
	    .iterateUpTo(
	        10,
	        [&childPid]() {
		        int status = 0;
		        auto pid = ::waitpid(childPid, &status, WNOHANG);
		        FAIL_IF(pid <= 0);
		        return LOOP_ASSERTION(WIFEXITED(status) && (WEXITSTATUS(status) == 0));
	        },
	        2s)
	    .hard_assert_passed();
}

namespace {
TestSuite _("mainTester",
            {
                CLASSY_TEST(callAndStopMain),
            });
} // namespace
} // namespace flexisip::tester
