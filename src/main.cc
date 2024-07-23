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

#include <flexisip/logmanager.hh>
#include <tclap/CmdLine.h>

#include "flexisip.hh"

using namespace std;
using namespace flexisip;

int main(int argc, char* argv[]) {
	try {
		return _main(argc, argv);
	} catch (const TCLAP::ExitException& exception) {
		// Exception raised when the program failed to correctly parse command line options.
		return exception.getExitStatus();
	} catch (const Exit& exception) {
		// If there are no explanatory string to print, exit now.
		if (exception.what() == nullptr or exception.what()[0] == '\0') {
			return exception.code();
		}
		if (exception.code() != EXIT_SUCCESS) {
			cerr << "Error, caught exit exception: " << exception.what() << endl;
			return exception.code();
		}

		SLOGD << "Exit success: " << exception.what();
		return exception.code();
	} catch (const exception& exception) {
		cerr << "Error, caught an unexpected exception: " << exception.what() << endl;
		return EXIT_FAILURE;
	} catch (...) {
		cerr << "Error, caught an unknown exception" << endl;
		return EXIT_FAILURE;
	}
}
