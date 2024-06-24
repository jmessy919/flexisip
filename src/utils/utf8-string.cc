/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "utf8-string.hh"

#include <cassert>
#include <cstring>
#include <sstream>
#include <string>

#include <iconv.h>

using namespace std::string_view_literals;

namespace {

constexpr auto replacementChar = "�"sv;

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

	size_t operator()(const char** inBuf, size_t* inBytesLeft, char** outBuf, size_t* outBytesLeft) {
		return iconv(mDescriptor, const_cast<char**>(inBuf), inBytesLeft, outBuf, outBytesLeft);
	}

private:
	iconv_t mDescriptor;
};

} // namespace

namespace flexisip::utils {

Utf8String::Utf8String(std::string_view source) {
	if (source.empty()) {
		// The empty string is already valid, nothing to do.
		return;
	}

	IConv converter("UTF-8", "UTF-8");
	size_t inBytesLeft = source.size();
	mData.resize(inBytesLeft);
	size_t outBytesLeft = mData.size();
	const char* pInBuf = source.data();
	char* pOutBuf = mData.data();

	while (0 < inBytesLeft) {
		if (converter(&pInBuf, &inBytesLeft, &pOutBuf, &outBytesLeft) != -1ul) {
			return;
		}

		const auto currentOffset = pOutBuf - mData.data();
		mData.resize(mData.size() + replacementChar.size());
		pOutBuf = mData.data() + currentOffset;
		std::memcpy(pOutBuf, replacementChar.data(), replacementChar.size());
		pOutBuf += replacementChar.size();

		// Move to the next byte, maybe the rest of the string is valid
		++pInBuf;
		--inBytesLeft;
	};
}

} // namespace flexisip::utils
