/** Copyright (C) 2010-2023 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "bctoolbox/tester.h"
#include "utils/http-mock/http-mock.hh"

#include <atomic>
#include <limits>
#include <memory>
#include <mutex>
#include <string>

#include <sys/types.h>

#include <nghttp2/nghttp2.h>

#include "flexisip/sofia-wrapper/su-root.hh"

#include "utils/test-patterns/test.hh"
#include "utils/test-suite.hh"
#include "utils/transport/http/http-headers.hh"
#include "utils/transport/http/http2client.hh"

using namespace std::string_literals;
using namespace std::chrono_literals;

namespace flexisip::tester {

void newTest() {
	struct UserData {
		ssize_t sendReturnValue{NGHTTP2_ERR_WOULDBLOCK};
	} userData{};
	nghttp2_session_callbacks* cbs;
	nghttp2_session_callbacks_new(&cbs);
	std::unique_ptr<nghttp2_session_callbacks, void (*)(nghttp2_session_callbacks*)> cbsPtr{
	    cbs, nghttp2_session_callbacks_del};
	nghttp2_session* session;
	nghttp2_session_client_new(&session, cbs, &userData);
	Http2Client::NgHttp2SessionPtr httpSession{session};
	nghttp2_session_callbacks_set_send_callback(
	    cbs, [](nghttp2_session*, const uint8_t*, size_t, int, void* user_data) noexcept {
		    return static_cast<UserData*>(user_data)->sendReturnValue;
	    });
	nghttp2_session_callbacks_set_on_frame_send_callback(cbs,
	                                                     [](nghttp2_session*, const nghttp2_frame*, void*) noexcept {
		                                                     SLOGD << "BEDUG " << __PRETTY_FUNCTION__;
		                                                     return 0;
	                                                     });
	nghttp2_session_callbacks_set_on_stream_close_callback(cbs,
	                                                       [](nghttp2_session*, int32_t, uint32_t, void*) noexcept {
		                                                       SLOGD << "BEDUG " << __PRETTY_FUNCTION__;
		                                                       return 0;
	                                                       });
	nghttp2_session_callbacks_set_recv_callback(cbs,
	                                            [](nghttp2_session*, uint8_t*, size_t, int, void*) noexcept -> ssize_t {
		                                            SLOGD << "BEDUG " << __PRETTY_FUNCTION__;
		                                            return 0;
	                                            });
	nghttp2_session_callbacks_set_on_frame_recv_callback(cbs,
	                                                     [](nghttp2_session*, const nghttp2_frame*, void*) noexcept {
		                                                     SLOGD << "BEDUG " << __PRETTY_FUNCTION__;
		                                                     return 0;
	                                                     });
	nghttp2_session_callbacks_set_on_header_callback(cbs, [](nghttp2_session*, const nghttp2_frame*, const uint8_t*,
	                                                         size_t, const uint8_t*, size_t, uint8_t, void*) noexcept {
		SLOGD << "BEDUG " << __PRETTY_FUNCTION__;
		return 0;
	});
	nghttp2_session_callbacks_set_on_data_chunk_recv_callback(
	    cbs, [](nghttp2_session*, uint8_t, int32_t, const uint8_t*, size_t, void*) noexcept {
		    SLOGD << "BEDUG " << __PRETTY_FUNCTION__;
		    return 0;
	    });

	Http2Client::HttpRequest request{{{
	                                     {"header-name", "header-value"},
	                                 }},
	                                 "test body"};
	nghttp2_submit_request(session, nullptr, request.getHeaders().makeCHeaderList().data(),
	                       request.getHeaders().getHeadersList().size(), request.getCDataProvider(), nullptr);
	SLOGD << "BEDUG sending";
	return;
	BC_HARD_ASSERT_CPP_EQUAL(nghttp2_session_send(session), 0);
}

void otherTest() {
	sofiasip::SuRoot root{};
	std::atomic_int requestsReceivedCount{0};
	HttpMock httpMock{{"/"}, &requestsReceivedCount};
	const auto port = [&httpMock]() {
		const auto port = httpMock.serveAsync();
		BC_HARD_ASSERT_TRUE(port > -1);
		return std::to_string(port);
	}();
	const auto client = Http2Client::make(root, "localhost", port);
	client->setRequestTimeout(1s);
	const HttpHeaders headers{
	    {":method"s, "POST"s},
	    {":scheme", "https"},
	    {":authority", "localhost:" + port},
	    {":path", "/"},
	};

	client->send(
	    std::make_shared<Http2Client::HttpRequest>(headers,
	                                               std::string(/* overflow window size */ 0x4009 * 4 - 0x25, 'A')),
	    [&root](const std::shared_ptr<Http2Client::HttpRequest>&, const std::shared_ptr<HttpResponse>&) {
		    BC_FAIL("This request will never be answered");
		    root.quit();
	    },
	    [&root](const std::shared_ptr<Http2Client::HttpRequest>&) {
		    root.quit();
	    });
	SLOGD << "BEDUG RUN";
	root.run();
	client->send(
	    std::make_shared<Http2Client::HttpRequest>(headers, "Trigger sending of remaining frames"),
	    [&root](const std::shared_ptr<Http2Client::HttpRequest>&, const std::shared_ptr<HttpResponse>&) {
		    root.quit();
	    },
	    [&root](const std::shared_ptr<Http2Client::HttpRequest>&) {
		    BC_FAIL("Unexpected error in resend trigger request");
		    root.quit();
	    });
	root.run();
	SLOGD << "BEDUG CLEANUP";
	root.step(1s);
	SLOGD << "BEDUG TEARDOWN";
}

namespace {
TestSuite _("Http2Client",
            {
                CLASSY_TEST(otherTest),
                CLASSY_TEST(newTest),
            });
}
} // namespace flexisip::tester
