/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "utf8-string.hh"

#include <sstream>
#include <string>

#include <iconv.h>

namespace {

// Thin wrapper around iconv* functions
class IConv {
public:
	IConv(const char* toCode, const char* fromCode) : mDescriptor(iconv_open(toCode, fromCode)) {
	}
	~IConv() {
		iconv_close(mDescriptor);
	}

	IConv(const IConv&) = delete;
	IConv(IConv&&) = delete;
	IConv& operator=(const IConv&) = delete;
	IConv& operator=(IConv&&) = delete;

	size_t operator()(char** inBuf, size_t* inBytesLeft, char** outBuf, size_t* outBytesLeft) {
		return iconv(mDescriptor, inBuf, inBytesLeft, outBuf, outBytesLeft);
	}

private:
	iconv_t mDescriptor;
};

} // namespace

namespace flexisip {

namespace utils {

Utf8String::Utf8String(const std::string& source) : mData(source) {
	IConv converter("UTF-8", "UTF-8");
	size_t inBytesLeft = mData.size();
	size_t outBytesLeft = inBytesLeft;
	char* pInBuf = &mData.front();
	char outBuf[outBytesLeft];
	char* pOutBuf = outBuf;
	if (converter(&pInBuf, &inBytesLeft, &pOutBuf, &outBytesLeft) != -1ul) {
		return;
	}

	std::ostringstream sanitized{};
	while (0 < inBytesLeft) {
		pOutBuf[0] = '\0';
		sanitized << outBuf << "�";
		pOutBuf = outBuf;
		++pInBuf;
		--inBytesLeft;
		converter(&pInBuf, &inBytesLeft, &pOutBuf, &outBytesLeft);
	};
	pOutBuf[0] = '\0';
	sanitized << outBuf;

	mData = sanitized.str();
}

} // namespace utils

} // namespace flexisip
