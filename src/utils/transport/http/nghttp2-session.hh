/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <sys/types.h>

#include <nghttp2/nghttp2.h>

namespace flexisip {

class Nghttp2Session {
public:
	class StreamId {
	public:
		friend class Nghttp2Session;

		class Hash {
			std::size_t operator()(const StreamId& self) {
				return std::hash<decltype(mId)>()(self.mId);
			}
		};

		bool operator==(StreamId other) {
			return mId == other.mId;
		}

	private:
		explicit StreamId(int32_t id) : mId(id) {
		}

		operator int32_t() const {
			return mId;
		}

		int32_t mId;
	};

	using StreamDataProvider = std::function<ssize_t(uint8_t* buf, size_t length, uint32_t& data_flags)>;

	Nghttp2Session();
	virtual ~Nghttp2Session() = default;

	StreamId submitRequest(const nghttp2_priority_spec*, const std::vector<nghttp2_nv>& headers, StreamDataProvider&&);

protected:
	virtual ssize_t onSend(const uint8_t* data, size_t length) noexcept = 0;
	virtual ssize_t onRecv(uint8_t* buf, size_t length) noexcept = 0;
	virtual int onFrameSent(const nghttp2_frame& frame) noexcept = 0;
	virtual int onFrameRecv(const nghttp2_frame& frame) noexcept = 0;
	virtual int onHeaderRecv(const nghttp2_frame& frame,
	                         std::basic_string_view<uint8_t> name,
	                         std::basic_string_view<uint8_t> value,
	                         uint8_t flags) noexcept = 0;
	virtual int onDataChunkRecv(uint8_t flags, StreamId, std::basic_string_view<uint8_t> data) noexcept = 0;
	virtual int onStreamClosed(StreamId, uint32_t error_code) noexcept = 0;

private:
	struct NgHttp2SessionDeleter {
		void operator()(nghttp2_session* ptr) const noexcept {
			nghttp2_session_del(ptr);
		}
	};
	using NgHttp2SessionPtr = std::unique_ptr<nghttp2_session, NgHttp2SessionDeleter>;

	NgHttp2SessionPtr mPtr;
	std::unordered_map<StreamId, StreamDataProvider, StreamId::Hash> mStreams{};
};

} // namespace flexisip
