/*
	Flexisip, a flexible SIP proxy server with media capabilities.
	Copyright (C) 2010-2016  Belledonne Communications SARL, All rights reserved.

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
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

#include "stats.hh"
#include "registrardb.hh"
#include "log/logmanager.hh"

Stats::Stats(const std::string &name) {
	mName = name;
	mRunning = false;
}

void Stats::start() {
	mRunning = true;
	pthread_create(&mThread, NULL, &Stats::threadfunc, this);
}

void Stats::stop() {
	if (mRunning) {
		mRunning = false;
		shutdown(mLocalSocket, SHUT_RDWR);
#if __APPLE__
		pthread_kill(mThread, SIGINT);
#else
		pthread_join(mThread, NULL);
#endif
	}
}

class StatFetchListener : public RegistrarDbListener {
	friend class Stats;
private:
	SofiaAutoHome mHome;
	unsigned int mSocket;
	std::string mAddress;

public:
	void onRecordFound(Record *r) {
		std::ostringstream answer;
		if(r) {
			std::list<std::shared_ptr<ExtendedContact>> contacts = r->getExtendedContacts();
			answer << "Identities of " << mAddress << " :";
			for(std::list<std::shared_ptr<ExtendedContact>>::iterator iter = contacts.begin(); iter != contacts.end(); ++iter) {
				answer << "\r\n\t <" << iter->get()->mSipUri << ">+sip.instance=" << iter->get()->mUniqueId << ";expire=" << iter->get()->mExpireAt;
			}
		} else {
			answer << "No identity were found for user : " << mAddress;
		}
		send(mSocket, answer.str().c_str(), answer.str().length(), 0);
		close(mSocket);
	}
	void onError() {
		std::ostringstream answer;
		answer << "Error while fetching identities of  user :" << mAddress;
		send(mSocket, answer.str().c_str(), answer.str().length(), 0);
		close(mSocket);
	}

	void onInvalid() {
		std::ostringstream answer;
		answer << "Error : Invalid user : " << mAddress;
		send(mSocket, answer.str().c_str(), answer.str().length(), 0);
		close(mSocket);
	}
};

static void split(const std::string &s, char delim, std::vector<std::string> &elems) {
	std::stringstream ss;
	ss.str(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		elems.push_back(item);
	}
}

static std::vector<std::string> split(const std::string &s, char delim) {
	std::vector<std::string> elems;
	split(s, delim, elems);
	return elems;
}

GenericEntry* Stats::find(GenericStruct *root, std::vector<std::string> &path) {
	std::string elem = path.front();
	path.erase(path.begin());
	for (auto it = root->getChildren().begin(); it != root->getChildren().end(); ++it) {
		GenericEntry *entry = *it;
		if (entry && entry->getName().compare(elem) == 0) {
			if (path.empty()) {
				return entry;
			} else {
				GenericStruct *gstruct = dynamic_cast<GenericStruct *>(entry);
				if (gstruct) {
					return find(gstruct, path);
				} else {
					return NULL;
				}
			}
		}
	}
	return NULL;
}

static std::string printEntry(GenericEntry *entry, bool printHelpInsteadOfValue) {
	GenericStruct *gstruct = dynamic_cast<GenericStruct *>(entry);
	bool isNode = gstruct != NULL;
	std::string answer = "";
	
	if (printHelpInsteadOfValue) {
		if (isNode) answer += "[";
		answer += entry->getName();
		if (isNode) answer += "]";
		answer += " : " + entry->getHelp();
	} else {
		if (isNode) {
			return "[" + gstruct->getName() + "]";
		} else {
			StatCounter64 *counter = dynamic_cast<StatCounter64 *>(entry);
			if (counter) {
				return counter->getName() + " : " + std::to_string(counter->read());
			} else {
				ConfigValue *value = dynamic_cast<ConfigValue *>(entry);
				if (value) {
					return value->getName() + " : " + value->get();
				}
			}
		}
	}
	return answer;
}

static std::string printSection(GenericStruct *gstruct, bool printHelpInsteadOfValue) {
	std::string answer = "";
	for (auto it = gstruct->getChildren().begin(); it != gstruct->getChildren().end(); ++it) {
		GenericEntry *child = *it;
		if (child) {
			answer += printEntry(child, printHelpInsteadOfValue) + "\r\n";
		}
	}
	return answer;
}

static void updateLogsVerbosity(GenericManager *manager) {
	std::string loglevel = manager->getGlobal()->get<ConfigString>("log-level")->read();
	std::string sysloglevel = manager->getGlobal()->get<ConfigString>("syslog-level")->read();
	bool user_errors = manager->getGlobal()->get<ConfigBoolean>("user-errors-logs")->read();
	flexisip::log::initLogs(flexisip_sUseSyslog, loglevel, sysloglevel, user_errors, false);
}

void Stats::parseAndAnswer(unsigned int socket, const std::string& query) {
	std::vector<std::string> query_split = split(query, ' ');
	std::ostringstream answer;
	answer << "Error: unknown error";
	int size = query_split.size();

	if (size < 2) {
		answer.str("");
		answer.clear();
		answer << "Error: at least 2 arguments were expected, got " << std::to_string(size);
	} else {
		std::string command = query_split.front();
		std::string arg = query_split.at(1);
		std::vector<std::string> arg_split = split(arg, '/');
		GenericManager *manager = GenericManager::get();
		GenericStruct *root = manager->getRoot();
		GenericEntry *entry = NULL;
		
		if (arg.compare("all") == 0) {
			entry = root;
		} else {
			entry = find(root, arg_split);
		}

		answer.str("");
		answer.clear();
		if (entry || "DUMP" == command) {
			if ("GET" == command) {
				GenericStruct *gstruct = dynamic_cast<GenericStruct *>(entry);
				if (gstruct) {
					answer << printSection(gstruct, false);
				} else {
					answer << printEntry(entry, false);
				}
			} else if ("LIST" == command) {
				GenericStruct *gstruct = dynamic_cast<GenericStruct *>(entry);
				if (gstruct) {
					answer << printSection(gstruct, true);
				} else {
					answer << printEntry(entry, true);
				}
			} else if ("SET" == command) {
				if (size < 3) {
					answer << "Error: at least 3 arguments were expected, got " << std::to_string(size);
				} else {
					std::string value = query_split.at(2);
					ConfigValue *config_value = dynamic_cast<ConfigValue *>(entry);
					if (config_value && "global/debug" == arg) {
						config_value->set(value);
						updateLogsVerbosity(manager);
						answer << "debug : " << value;
					} else if (config_value && "global/log-level" == arg) {
						config_value->set(value);
						updateLogsVerbosity(manager);
						answer << "log-level : " << value;
					} else if (config_value && "global/syslog-level" == arg) {
						config_value->set(value);
						updateLogsVerbosity(manager);
						answer << "syslog-level : " << value;
					} else {
						answer << "Only debug, log-level and syslog-level from global can be updated while flexisip is running";
					}
				}
			} else if ("DUMP" == command) {
				if ("all" == arg) {
					std::map<std::string,time_t> myMap = RegistrarDb::get()->getLocalExpiresMap();
					answer << "List of all registered users :";
					for(std::map<std::string,time_t>::iterator iter = myMap.begin(); iter != myMap.end(); ++iter) {
						answer << "\r\n\t sip:" << iter->first;
					}
				} else {
					if(arg.at(0) == '<') {
						arg = arg.substr(1, arg.length()-2);
					}
					auto listener = make_shared<StatFetchListener>();
					listener->mSocket = socket;
					listener->mAddress = arg;
					url_t *user = url_format(listener->mHome.home(), "%s", arg.c_str());
					RegistrarDb::get()->fetch(user, listener, true);
					return;
				}
			} else {
				answer << "Error: unknown command " << command;
			}
		} else {
			answer << "Error: " << arg << " not found";
		}
	}
	send(socket, answer.str().c_str(), answer.str().length(), 0);
	close(socket);
}

void Stats::run() {
	int remote_socket;
	unsigned int remote_length;
	struct sockaddr_un local, remote;
	if ((mLocalSocket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		LOGE("Socket error %i : %s", errno, std::strerror(errno));
		stop();
	}
	memset(&local, 0, sizeof(local));
	local.sun_family = AF_UNIX;
	int pid = getpid();
	std::string path = "/tmp/flexisip-" + mName + "-" + std::to_string(pid);
	SLOGD << "Statistics socket is at " << path;
	strncpy(local.sun_path, path.c_str(), sizeof(local.sun_path) -1);
	unlink(local.sun_path);
	if (::bind(mLocalSocket, (struct sockaddr *)&local, sizeof(local)) == -1) {
		LOGE("Bind error%i : %s", errno, std::strerror(errno));
		stop();
	}

	if (listen(mLocalSocket, 1) == -1) {
		LOGE("Listen error %i : %s", errno, std::strerror(errno));
		stop();
	}
	
	while (mRunning) {
		remote_length = sizeof(remote);
		if ((remote_socket = accept(mLocalSocket, (struct sockaddr *)&remote, &remote_length)) == -1) {
			if (mRunning) LOGE("Accept error %i : %s", errno, std::strerror(errno));
			continue;
		}
		
		bool finished = false;
		do {
		    char buffer[512]={0};
		    int n = recv(remote_socket, buffer, sizeof(buffer)-1, 0);
		    if (n < 0) {
				LOGE("Recv error %i : %s", errno, std::strerror(errno));
		    }
		    if (n > 0) {
				LOGD("[Stats] Received: %s", buffer);
				parseAndAnswer(remote_socket, buffer);
		    }
		    finished = true;
		} while (!finished && mRunning);
	}
	close(mLocalSocket);
	unlink(path.c_str());
}

void *Stats::threadfunc(void *arg) {
	Stats *thiz = (Stats *)arg;
	thiz->run();
	return NULL;
}

Stats::~Stats() {
	if (mRunning) {
		stop();
	}
}
