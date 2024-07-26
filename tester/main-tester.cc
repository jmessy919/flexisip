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

#include "utils/test-patterns/test.hh"
#include "utils/test-suite.hh"

#include <iostream>

#include "flexisip.hh"
#include "flexisip/logmanager.hh"
#include "utils/posix-process.hh"
#include "utils/variant-utils.hh"

using namespace std;

namespace flexisip::tester {

void findLogInFlexisipOutput(process::ExitedNormally* exitedNormally) {
	bool proxyStarted{};
	string fullLog{};
	string previousChunk{};
	constexpr auto timeout = 10s;
	const auto deadline = chrono::system_clock::now() + timeout;
	do {
		auto& standardOut = EXPECT_VARIANT(pipe::ReadOnly&).in(exitedNormally->mStdout);
		const auto& chunk = [&standardOut, &fullLog] {
			try {
				return EXPECT_VARIANT(string).in(standardOut.read(0xFF));
			} catch (const exception& exc) {
				ostringstream msg{};
				msg << "Something went wrong reading flexisip stdout: " << exc.what()
				    << ". Read so far ('|' indicates chunk boundaries): " << fullLog;
				throw runtime_error{msg.str()};
			}
		}();
		const auto& concatenated = previousChunk + chunk;
		// TODO: Choose which logs to look at. Maybe more than one log if there are multiple services.
		//  kind of array with logs search, service type and if its found or not?
		if (!proxyStarted && concatenated.find("Starting flexisip proxy-server") != string::npos) {
			proxyStarted = true;
		}
		if (!chunk.empty()) fullLog += "|" + chunk;
		previousChunk = std::move(chunk);
	} while (chrono::system_clock::now() < deadline);
	cout << fullLog;
	fullLog = "";
	const auto deadlineerr = chrono::system_clock::now() + timeout;
	do {
		auto& standardOut = EXPECT_VARIANT(pipe::ReadOnly&).in(exitedNormally->mStderr);
		const auto& chunk = [&standardOut, &fullLog] {
			try {
				return EXPECT_VARIANT(string).in(standardOut.read(0xFF));
			} catch (const exception& exc) {
				ostringstream msg{};
				msg << "Something went wrong reading flexisip stdout: " << exc.what()
				    << ". Read so far ('|' indicates chunk boundaries): " << fullLog;
				throw runtime_error{msg.str()};
			}
		}();
		const auto& concatenated = previousChunk + chunk;
		// TODO: Choose which logs to look at. Maybe more than one log if there are multiple services.
		//  kind of array with logs search, service type and if its found or not?
		if (!proxyStarted && concatenated.find("Starting flexisip proxy-server") != string::npos) {
			proxyStarted = true;
		}
		if (!chunk.empty()) fullLog += "|" + chunk;
		previousChunk = std::move(chunk);
	} while (chrono::system_clock::now() < deadlineerr);

	cout << fullLog;

	BC_ASSERT_TRUE(proxyStarted);
}

/** TODO: Replace with test description
 * A note on deadlocks:
 *
 * The ProxyCommandLineInterface spawns its own thread to accept connections on the Unix socket.
 * For our test purposes, we connect to that socket, send a command, and wait to receive a reply (blocking that thread).
 * The CLI thread accepts and handles the command, but when using Redis, will initiate an async exchange. It will then
 * be up to the sofia loop to step through the rest of the operations with Redis. Only at the end of that exchange will
 * the temp socket be closed, and the thread that sent the command released. If that thread is the same as (or waiting
 * on) the one that created the sofia loop, then it's a deadlock, since it can't possibly iterate the loop.
 */
void callAndStopMain() {
	vector<string> args{
	    "flexisip",
	    "-c /home/ndelpech/Development/clion_flexisip/cmake-build-debug/install/etc/flexisip/flexisip.conf",
	};
	// TODO: More c++-like definition of args for use of modifications?
	char arg0[] = "flexisip";
	char arg1[] = "-c";
	char arg2[] = "/home/ndelpech/Development/clion_flexisip/cmake-build-debug/install/etc/flexisip/flexisip.conf";
	char arg3[] = "-d";
	// TODO: Set and write conf to test flexisip services
	//  and set the log output to something that can be caught in the parent process
	char* argv[] = {&arg0[0], &arg1[0], &arg2[0], &arg3[0], nullptr};
	int argc = sizeof(argv) / sizeof(char*) - 1;

	process::Process test([&argc, &argv]() { exit(_main(argc, argv)); });

	auto* running = get_if<process::Running>(&test.state());
	BC_HARD_ASSERT_NOT_NULL(running);
	// Parent process
	// TODO: Loop iterate until log stating that flexisip started properly is received
	//  then or if not received sent sigint to child
	//  or child autokills himself with timer and callback sending sigint and check if child process still living

	// TODO: Which log should we check?

	sleep(5);
	auto errCode = running->signal(SIGINT);
	BC_HARD_ASSERT_CPP_EQUAL(errCode.value_or(SysErr()).number(), 0);

	// TODO: iterate loop and exit after 1 or 2 sec if still not exited=

	auto finished = std::move(test).wait();
	auto* exitedNormally = get_if<process::ExitedNormally>(&finished);
	BC_ASSERT_PTR_NOT_NULL(exitedNormally);

	// Check logs to see if the server started and stopped properly
	findLogInFlexisipOutput(exitedNormally);

	auto* out = get_if<pipe::ReadOnly>(&exitedNormally->mStdout);
	BC_ASSERT_PTR_NOT_NULL(out);
	auto maybeRead = out->read(0xFFFF);
	auto* read = get_if<string>(&maybeRead);
	SLOGD << "exitCode" << (int)exitedNormally->mExitCode << " inside " << read->c_str() << endl;
	auto* err = get_if<pipe::ReadOnly>(&exitedNormally->mStderr);
	BC_ASSERT_PTR_NOT_NULL(err);
	cerr << "errors:" << endl;
	maybeRead = err->read(0xFF);
	auto* readErr = get_if<string>(&maybeRead);
	cerr << "error " << endl << *readErr << endl;
	BC_ASSERT_CPP_EQUAL((int)exitedNormally->mExitCode, EXIT_SUCCESS);
	// TODO: Test that return value == EXIT_SUCCESS with timeout
}

namespace {
TestSuite _("mainTester",
            {
                CLASSY_TEST(callAndStopMain),
            });
} // namespace
} // namespace flexisip::tester
