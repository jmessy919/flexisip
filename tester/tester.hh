/*
 Flexisip, a flexible SIP proxy server with media capabilities.
 Copyright (C) 2010-2021  Belledonne Communications SARL, All rights reserved.

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as
 published by the Free Software Foundation, either version 3 of the
 License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef flexisip_tester_hpp
#define flexisip_tester_hpp

extern "C" {
#include "bctoolbox/tester.h"
}
#include "flexisip/agent.hh"
#include <linphone++/linphone.hh>
#include "flexisip/sofia-wrapper/su-root.hh"

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <functional>
#include <list>
#include <thread>
#include <chrono>

std::string bcTesterFile(const std::string& name);
std::string bcTesterRes(const std::string& name);

#ifdef __cplusplus
extern "C" {
#endif

extern test_suite_t agent_suite;
extern test_suite_t boolean_expressions_suite;
extern test_suite_t conference_suite;
extern test_suite_t extended_contact_suite;
extern test_suite_t fork_context_suite;
extern test_suite_t fork_context_mysql_suite;
extern test_suite_t module_pushnitification_suite;
extern test_suite_t push_notification_suite;
extern test_suite_t register_suite;
extern test_suite_t registration_event_suite;
extern test_suite_t router_suite;
extern test_suite_t tls_connection_suite;
#if ENABLE_B2BUA
extern test_suite_t b2bua_suite;
#endif

void flexisip_tester_init(void (*ftester_printf)(int level, const char* fmt, va_list args));
void flexisip_tester_uninit(void);

#ifdef __cplusplus
};
#endif

	class BcAssert {
	public:
		void addCustomIterate(const std::function<void ()> &iterate) {
			mIterateFuncs.push_back(iterate);
		}
		bool waitUntil( std::chrono::duration<double> timeout ,const std::function<bool ()> &condition) {
			auto start = std::chrono::steady_clock::now();

			bool result;
			while (!(result = condition()) && (std::chrono::steady_clock::now() - start < timeout)) {
				for (const auto &iterate:mIterateFuncs) {
					iterate();
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
			return result;
		}
		bool wait(const std::function<bool ()> &condition) {
			return waitUntil(std::chrono::seconds(2),condition);
		}
	private:
		std::list<std::function<void ()>> mIterateFuncs;
	};

	class CoreAssert : public BcAssert {
	public:
		CoreAssert(std::initializer_list<std::shared_ptr<linphone::Core>> cores) {
			for (std::shared_ptr<linphone::Core> core: cores) {
				addCustomIterate([core] {
					core->iterate();
				});
			}
		}
		CoreAssert(std::initializer_list<std::shared_ptr<linphone::Core>> cores, std::shared_ptr<flexisip::Agent> agent) : CoreAssert(cores) {
			addCustomIterate([agent] { agent->getRoot()->step(std::chrono::milliseconds(1) ); });
		}
	};

	/**
	 * A class to manage the flexisip proxy server
	 */
	class Server {
	private:
		std::shared_ptr<sofiasip::SuRoot> mRoot;
		std::shared_ptr<flexisip::Agent> mAgent;

	public:
		// Accessors
		std::shared_ptr<sofiasip::SuRoot> getRoot() noexcept {
			return mRoot;
		}

		std::shared_ptr<flexisip::Agent> getAgent() noexcept {
			return mAgent;
		}

		void start() {
			mAgent->start("", "");
		}

		/**
		 * Create the sofiasip root, the Agent and load the config file given as parameter
		 *
		 * @param[in] configFile	The path to config file. Search for it in the resource directory and TESTER_DATA_DIR
		 */
		Server(const std::string& configFile=std::string());
		~Server();
	}; // Class Server

	/**
	 * Class to manage a client Core
	 */
	class CoreClient {
	private:
		std::shared_ptr<linphone::Core> mCore;
		std::shared_ptr<linphone::Account> mAccount;
		std::shared_ptr<const linphone::Address> mMe;
		std::shared_ptr<Server> mServer; /**< Server we're registered to */

	public:
		std::shared_ptr<linphone::Core> getCore() noexcept {
			return mCore;
		}
		std::shared_ptr<linphone::Account> getAccount() noexcept {
			return mAccount;
		}
		std::shared_ptr<const linphone::Address> getMe() noexcept {
			return mMe;
		}

		/**
		 * create and start client core
		 *
		 * @param[in] me	address of local account
		 */
		CoreClient(std::string me);

		/**
		 * Create and start client core, create an account and register to given server
		 *
		 * @param[in] me		address of local account
		 * @param[in] server	server to register to
		 */
		CoreClient(std::string me, std::shared_ptr<Server> server);

		/**
		 * Create an account(using address given at client creation) and register to the given server
		 *
		 * @param[in] server	server to register to
		 */
		void registerTo(std::shared_ptr<Server> server);

		~CoreClient();

		/**
		 * Establish a call
		 *
		 * @param[in] callee 			client to call
		 * @param[in] callerCallParams	call params used by the caller to answer the call. nullptr to use default callParams
		 * @param[in] calleeCallParams	call params used by the callee to accept the call. nullptr to use default callParams
		 *
		 * @return the established call from caller side, nullptr on failure
		 */
		std::shared_ptr<linphone::Call> call(std::shared_ptr<CoreClient> callee,
											std::shared_ptr<linphone::CallParams> callerCallParams=nullptr,
											std::shared_ptr<linphone::CallParams> calleeCallParams=nullptr);

		/**
		 * Establish a video call.
		 * video is enabled caller side, with or without callParams given
		 *
		 * @param[in] callee 			client to call
		 * @param[in] callerCallParams	call params used by the caller to answer the call. nullptr to use default callParams
		 * @param[in] calleeCallParams	call params used by the callee to accept the call. nullptr to use default callParams
		 *
		 * @return the established call from caller side, nullptr on failure
		 */
		std::shared_ptr<linphone::Call> callVideo(std::shared_ptr<CoreClient> callee,
											std::shared_ptr<linphone::CallParams> callerCallParams=nullptr,
											std::shared_ptr<linphone::CallParams> calleeCallParams=nullptr);

		/**
		 * Update an ongoing call.
		 * When enable/disable video, check that it is correctly executed on both sides
		 *
		 * @param[in] peer				peer clientCore involved in the call
		 * @param[in] callerCallParams	new call params to be used by self
		 *
		 * @return true if all asserts in the callUpdate succeded, false otherwise
		 */
		bool callUpdate(std::shared_ptr<CoreClient> peer, std::shared_ptr<linphone::CallParams> callerCallParams);

		/**
		 * Get from the two sides the current call and terminate if from this side
		 * assertion failed if one of the client is not in a call or both won't end into Released state
		 *
		 * @param[in]	peer	The other client involved in the call
		 *
		 * @return true if all asserts in the function succeded, false otherwise
		 */
		bool endCurrentCall(std::shared_ptr<CoreClient> peer);

	}; // class CoreClient

#endif
