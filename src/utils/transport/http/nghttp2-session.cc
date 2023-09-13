/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "nghttp2-session.hh"
#include <cstdint>
#include <nghttp2/nghttp2.h>
#include <sstream>
#include <sys/types.h>

namespace flexisip {

Nghttp2Session::Nghttp2Session() : mPtr(nullptr) {
	static const auto sCallbacks = []() {
		nghttp2_session_callbacks* cbs;
		nghttp2_session_callbacks_new(&cbs);
		nghttp2_session_callbacks_set_send_callback(
		    cbs, [](nghttp2_session*, const uint8_t* data, size_t length, int, void* user_data) noexcept {
			    return static_cast<Nghttp2Session*>(user_data)->onSend(data, length);
		    });
		nghttp2_session_callbacks_set_recv_callback(
		    cbs, [](nghttp2_session*, uint8_t* buf, size_t length, int, void* user_data) noexcept {
			    return static_cast<Nghttp2Session*>(user_data)->onRecv(buf, length);
		    });
		nghttp2_session_callbacks_set_on_frame_send_callback(
		    cbs, [](nghttp2_session*, const nghttp2_frame* frame, void* user_data) noexcept {
			    return static_cast<Nghttp2Session*>(user_data)->onFrameSent(*frame);
		    });
		nghttp2_session_callbacks_set_on_frame_recv_callback(
		    cbs, [](nghttp2_session*, const nghttp2_frame* frame, void* user_data) noexcept {
			    return static_cast<Nghttp2Session*>(user_data)->onFrameRecv(*frame);
		    });
		nghttp2_session_callbacks_set_on_header_callback(
		    cbs, [](nghttp2_session*, const nghttp2_frame* frame, const uint8_t* name, size_t namelen,
		            const uint8_t* value, size_t valuelen, uint8_t flags, void* user_data) noexcept {
			    return static_cast<Nghttp2Session*>(user_data)->onHeaderRecv(*frame, {name, namelen}, {value, valuelen},
			                                                                 flags);
		    });
		nghttp2_session_callbacks_set_on_data_chunk_recv_callback(cbs, [](nghttp2_session*, uint8_t flags,
		                                                                  int32_t stream_id, const uint8_t* data,
		                                                                  size_t len, void* user_data) noexcept {
			return static_cast<Nghttp2Session*>(user_data)->onDataChunkRecv(flags, StreamId(stream_id), {data, len});
		});
		nghttp2_session_callbacks_set_on_stream_close_callback(
		    cbs, [](nghttp2_session*, int32_t stream_id, uint32_t error_code, void* user_data) noexcept {
			    return static_cast<Nghttp2Session*>(user_data)->onStreamClosed(StreamId(stream_id), error_code);
		    });

		return std::unique_ptr<nghttp2_session_callbacks, void (*)(nghttp2_session_callbacks*)>{
		    cbs, nghttp2_session_callbacks_del};
	}();

	nghttp2_session* session;
	nghttp2_session_client_new(&session, sCallbacks.get(), this);
	mPtr.reset(session);
}

Nghttp2Session::StreamId Nghttp2Session::submitRequest(const nghttp2_priority_spec* prioritySpec,
                                                       const std::vector<nghttp2_nv>& headers,
                                                       StreamDataProvider&& dataProvider) {
	static const nghttp2_data_provider sDataProvider{
	    .read_callback = [](nghttp2_session* session, int32_t stream_id, uint8_t* buf, size_t length,
	                        uint32_t* data_flags, nghttp2_data_source*, void* user_data) noexcept -> ssize_t {
		    const auto& dataProviders = *static_cast<decltype(mStreams)*>(user_data);
		    const auto dataProvider = dataProviders.find(StreamId(stream_id));
		    if (dataProvider == dataProviders.end()) {
			    nghttp2_submit_rst_stream(session, nghttp2_flag::NGHTTP2_FLAG_NONE, stream_id,
			                              nghttp2_error::NGHTTP2_ERR_CANCEL);
			    return nghttp2_error::NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
		    }

		    return (dataProvider->second)(buf, length, *data_flags);
	    },
	};

	const StreamId submitted{
	    nghttp2_submit_request(mPtr.get(), prioritySpec, headers.data(), headers.size(), &sDataProvider, &mStreams)};

	mStreams.emplace(submitted, std::move(dataProvider));

	return submitted;
}

} // namespace flexisip
