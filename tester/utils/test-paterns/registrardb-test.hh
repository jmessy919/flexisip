/*  SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#include "../redis-server.hh"
#include "agent-test.hh"

using namespace std;

namespace flexisip {
namespace tester {

namespace DbImplementation {

class Internal {
public:
	void amendConfiguration(GenericManager& cfg) {
		auto* registrarConf = cfg.getRoot()->get<GenericStruct>("module::Registrar");
		registrarConf->get<ConfigValue>("db-implementation")->set("internal");
	}
};

class Redis {
	RedisServer mRedisServer{};

public:
	int mPort = -1;

	void amendConfiguration(GenericManager& cfg) {
		mPort = mRedisServer.start();

		auto* registrarConf = cfg.getRoot()->get<GenericStruct>("module::Registrar");
		registrarConf->get<ConfigValue>("db-implementation")->set("redis");
		registrarConf->get<ConfigValue>("redis-server-domain")->set("localhost");
		registrarConf->get<ConfigValue>("redis-server-port")->set(to_string(mPort));
	}
};

} // namespace DbImplementation

template <typename TDatabase>
class RegistrarDbTest : public AgentTest {
public:
	RegistrarDbTest(bool startAgent = false) noexcept : AgentTest(startAgent) {
	}

	void onAgentConfiguration(GenericManager& cfg) override {
		AgentTest::onAgentConfiguration(cfg);
		dbImpl.amendConfiguration(cfg);
	}

protected:
	TDatabase dbImpl;
};

} // namespace tester
} // namespace flexisip
