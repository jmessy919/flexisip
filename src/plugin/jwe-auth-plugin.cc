/*
	Flexisip, a flexible SIP proxy server with media capabilities.
	Copyright (C) 2010-2015  Belledonne Communications SARL, All rights reserved.

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

#include <array>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <list>
#include <string>
#include <vector>

extern "C" {
	#include <jose/jose.h>
}

// =============================================================================

using namespace std;

namespace {
	constexpr int JweAuthPluginVersion = 1;
	constexpr char JweAuthPluginName[] = "JWE Authentification plugin";
	constexpr char JwkFileExtension[] = "jwk";

	// TODO: Remove me.
	const string JwksPath("/home/rabhamon/jwks/");
}

// =============================================================================
// Generic helpers.
// =============================================================================

template<typename... T>
static constexpr auto makeArray(T &&...values) -> array<
	typename decay<typename common_type<T...>::type>::type,
	sizeof...(T)
> {
	return array<
		typename decay<typename common_type<T...>::type>::type,
		sizeof...(T)
	>{ forward<T>(values)... };
}

static bool endsWith(const string &str, const string &suffix) {
	const string::size_type strSize = str.size();
	const string::size_type suffixSize = suffix.size();
	return strSize >= suffixSize && str.compare(strSize - suffixSize, suffixSize, suffix) == 0;
}

static vector<char> readFile(const string &path, bool *error = nullptr) {
	vector<char> buf;

	fstream stream(path, ios_base::in);
	if (stream.is_open()) {
		stream.exceptions(ios::failbit);
		try {
			stream.seekg(0, ios::end);
			fstream::pos_type len(stream.tellg());

			stream.seekg(0, ios::beg);
			buf.resize(len);
			stream.read(buf.data(), len);

			if (error) *error = false;
			return buf;
		} catch (const ios_base::failure &e) {
			cerr << "Unable to read properly file (I/O error): `" << e.what() << "`." << endl;
		} catch (const exception &e) {
			cerr << "Unable to read properly file: `" << e.what() << "`." << endl;
		}
	} else
		cerr << "Unable to open: `" << path << "`." << endl;

	if (error) *error = true;
	return buf;
}

static list<string> listFiles(const string &path, const string &suffix) {
	list<string> files;
	dirent *dirent;

	DIR *dirp = opendir(path.c_str());
	if (!dirp) {
		cerr << "Unable to open directory: `" << path << "` (" << strerror(errno) << ")." << endl;
		return files;
	}

	const string dotSuffix = "." + suffix;
	for (;;) {
		errno = 0;
		if (!(dirent = readdir(dirp))) {
			if (errno)
				cerr << "Unable to read directory: `" << path << "` (" << strerror(errno) << ")." << endl;
			break;
		}

		string file(dirent->d_name);
		if (file != "." && file != ".." && endsWith(file, dotSuffix))
			files.push_back(move(file));
	}

	closedir(dirp);

	return files;
}

// =============================================================================
// JWT parser.
// =============================================================================

// Note:
// jwe => JSON Web Encryption
// jwk => JSON Web Key
// jwt => JSON Web Token

static json_t *convertToJson(const char *text, size_t len) {
	json_error_t error;
	json_t *root = json_loadb(text, len, 0, &error);
	if (root)
		return root;

	cerr << "Unable to convert to json, error line " << error.line << ": `" << error.text << "`." << endl;
	return nullptr;
}

static bool isB64(const char *text, size_t len) {
	for (size_t i = 0; i < len; ++i)
		if (text[i] && !strchr(JOSE_B64_MAP, text[i]))
			return false;
	return true;
}

static json_t *parseJwe(const char *text) {
	const auto parts = makeArray("protected", "encrypted_key", "iv", "ciphertext", "tag");
	const size_t nParts = parts.size();

	json_auto_t *jwe = json_object();
	for (size_t i = 0, j = 0; j < nParts; j++) {
		const char *separator = strchr(&text[i], '.');

		size_t len;
		if (separator)
			len = separator - &text[i];
		else if (j + 1 == nParts)
			len = strlen(&text[i]);
		else
			goto error;

		if (!isB64(&text[i], len))
			goto error;

		if (json_object_set_new(jwe, parts[j], json_stringn(&text[i], len)) < 0)
			goto error;

		i += len + 1;
	}

	return json_incref(jwe);

error:
	cerr << "Unable to parse JWE correctly." << endl;
	return nullptr;
}

static json_t *decryptJwe(const char *text, const json_t *jwk) {
	json_auto_t *jwe = parseJwe(text);
	if (!jwe)
		return nullptr;

	size_t len = 0;
	char *jwtText = static_cast<char *>(jose_jwe_dec(nullptr, jwe, nullptr, jwk, &len));
	if (!jwtText) {
		cerr << "Unable to decrypt JWE." << endl;
		return nullptr;
	}

	json_t *jwt = convertToJson(jwtText, len);
	free(jwtText);
	return jwt;
}

template<typename T>
struct TypeToJsonFormat {};

template<>
struct TypeToJsonFormat<int> {
	static constexpr char value = 'i';
};

template<>
struct TypeToJsonFormat<json_int_t> {
	static constexpr char value = 'I';
};

template<typename T>
static T extractJsonValue (json_t *jwt, const char *attrName, bool *error = nullptr) {
	constexpr char format[] = { '{', 's', ':', TypeToJsonFormat<T>::value, '}', '\0' };

	T value{};
	bool soFarSoGood = true;

	if (json_unpack(jwt, format, attrName, &value) < 0) {
		cerr << "Unable to unpack value: `" << attrName << "`." << endl;
		soFarSoGood = false;
	}

	if (error) *error = soFarSoGood;
	return value;
}

template<typename T>
static T extractJsonOptionalValue (json_t *jwt, const char *attrName, const T &defaultValue, bool *error = nullptr) {
	constexpr char format[] = { TypeToJsonFormat<T>::value, '\0' };

	T value(defaultValue);
	bool soFarSoGood = true;

	json_t *valueObject;
	if (json_unpack(jwt, "{s?o}", attrName, &valueObject) < 0) {
		cerr << "Unable to unpack optional object: `" << attrName << "`." << endl;
		soFarSoGood = false;
	} else if (valueObject && json_unpack(valueObject, format, &value) < 0) {
		cerr << "Unable to unpack existing value: `" << attrName << "`." << endl;
		soFarSoGood = false;
	}

	if (error) *error = soFarSoGood;
	return value;
}

static bool checkJwtTime(json_t *jwt) {
	const time_t currentTime = time(nullptr);
	bool error;

	// Check optional "exp" attr.
	const json_int_t expValue = extractJsonOptionalValue<json_int_t>(jwt, "exp", -1, &error);
	if (error)
		return false;
	if (expValue != -1 && time_t(expValue) < currentTime) {
		cerr << "JWT (exp) has expired." << endl;
		return false;
	}

	// Not in the JSON Web Token RFC. Check Specific optional "exp_in" attr.
	const int expInValue = extractJsonOptionalValue<int>(jwt, "exp_in", -1, &error);
	if (error)
		return false;
	if (expInValue != -1) {
		json_int_t iatValue = extractJsonValue<int>(jwt, "iat", &error);
		if (error) {
			cerr << "`exp_in` can be used only if `iat` exists." << endl;
			return false;
		}

		if (time_t(iatValue) + time_t(expInValue * 1000) < currentTime) {
			cerr << "JWT (exp_in) has expired." << endl;
			return false;
		}
	}

	return true;
}

// =============================================================================
// Plugin.
// =============================================================================

#include "plugin.hh"

class JweAuth : public Module {
public:
	JweAuth (Agent *agent);
	~JweAuth ();

private:
	json_t *decryptJwe(const string &text) const;
	bool checkJwt(json_t *jwt, const shared_ptr<const RequestSipEvent> &ev) const;

	void onRequest(shared_ptr<RequestSipEvent> &ev) override;
	void onResponse(shared_ptr<ResponseSipEvent> &ev) override;

	list<json_t *> mJwks;
};

FLEXISIP_DECLARE_PLUGIN(JweAuth, JweAuthPluginName, JweAuthPluginVersion);

// -----------------------------------------------------------------------------

static bool checkJwtAttrFromSipHeader(
	json_t *jwt,
	const shared_ptr<const SipEvent> &ev,
	const char *attrName,
	const char *sipHeaderName
) {
	char *value = nullptr;
	bool soFarSoGood = false;
	if (json_unpack(jwt, "{s:s}", attrName, &value) == 0) {
		sip_unknown_t *header = ModuleToolbox::getCustomHeaderByName(ev->getSip(), sipHeaderName);
		if (header && header->un_value)
			soFarSoGood = !!strcmp(value, header->un_value);
	}

	if (!soFarSoGood)
		cerr << "`" << attrName << " value ` not equal to `" << sipHeaderName << "`" << endl;

	free(value);
	return soFarSoGood;
}

// -----------------------------------------------------------------------------

JweAuth::JweAuth(Agent *agent) : Module(agent) {
	for (const string &file : listFiles(JwksPath, JwkFileExtension)) {
		bool error;
		const vector<char> buf(readFile(JwksPath + "/" + file, &error));
		if (!error) {
			json_t *jwk = convertToJson(buf.data(), buf.size());
			if (jwk)
				mJwks.push_back(jwk);
		}
	}
}

JweAuth::~JweAuth() {
	for (json_t *jwk : mJwks)
		json_decref(jwk);
}

json_t *JweAuth::decryptJwe(const string &text) const {
	for (const json_t *jwk : mJwks) {
		json_auto_t *jwt = ::decryptJwe(text.c_str(), jwk);
		if (jwt)
			return jwt;
	}
	return nullptr;
};

bool JweAuth::checkJwt(json_t *jwt, const shared_ptr<const RequestSipEvent> &ev) const {
	// 1. Check expiration time.
	if (!checkJwtTime(jwt))
		return false;

	// 2. Find incoming subject.
	const sip_unknown_t *header = ModuleToolbox::getCustomHeaderByName(ev->getSip(), "X-ticked-oid");
	if (!header || !header->un_value || !ev->findIncomingSubject(header->un_value)) {
		cerr << "Unable to find oid incoming subject in message." << endl;
		return false;
	}

	// 3. Check attributes.
	static constexpr auto toCheck = {
		pair<const char *, const char *>{ "oid", "X-ticked-oid" },
		{ "aud", "X-ticked-aud" },
		{ "req_act", "X-ticked-req_act" }
	};
	for (const auto &data : toCheck) {
		if (!checkJwtAttrFromSipHeader(jwt, ev, data.first, data.second))
			return false;
	}

	return true;
}

void JweAuth::onRequest(shared_ptr<RequestSipEvent> &ev) {


}

void JweAuth::onResponse(shared_ptr<ResponseSipEvent> &ev) {
	// Nothing.
}
