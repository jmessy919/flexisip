/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <sys/types.h>

#include <nghttp2/nghttp2.h>

namespace flexisip {

/**
 * Memory-safe low-level wrapper for a nghttp2_session in client mode
 */
class Nghttp2ClientSession {
public:
	using StreamId = int32_t;
	using StreamIdOrError = int32_t;

	class StreamDataProvider {
	public:
		virtual ~StreamDataProvider() = default;
		virtual ssize_t read(uint8_t* buf, size_t length, uint32_t& data_flags) = 0;
	};

	struct SessionSettings {
		std::optional<uint32_t> maxConcurrentStreams = std::nullopt;
	};

	Nghttp2ClientSession();
	virtual ~Nghttp2ClientSession() = default;

	StreamIdOrError submitRequest(const nghttp2_priority_spec*,
	                              const std::vector<nghttp2_nv>& headers,
	                              std::unique_ptr<StreamDataProvider>&&);
	int submitSettings(const SessionSettings&);
	StreamDataProvider* getStreamData(StreamId) const;
	void cancel(StreamId);
	int sendPendingFrames();
	int receiveRemoteFrames();
	/**
	 * Number of requests pending to be sent by the nghttp2 session
	 */
	size_t getOutboundQueueSize() {
		return nghttp2_session_get_outbound_queue_size(mPtr.get());
	}

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
	virtual int onStreamClosed(std::unique_ptr<StreamDataProvider>&&, uint32_t error_code) noexcept = 0;

private:
	struct NgHttp2SessionDeleter {
		void operator()(nghttp2_session* ptr) const noexcept {
			nghttp2_session_del(ptr);
		}
	};
	using NgHttp2SessionPtr = std::unique_ptr<nghttp2_session, NgHttp2SessionDeleter>;

	NgHttp2SessionPtr mPtr;
};

} // namespace flexisip
