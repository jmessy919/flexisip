/*
 Flexisip, a flexible SIP proxy server with media capabilities.
 Copyright (C) 2010-2023 Belledonne Communications SARL, All rights reserved.

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as
 published by the Free Software Foundation, either version 3 of the
 License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <functional>
#include <list>
#include <map>

#include <nghttp2/nghttp2.h>
#include <openssl/ssl.h>
#include <sofia-sip/su_wait.h>

#include <flexisip/sofia-wrapper/timer.hh>
#include <string_view>

#include "http-message-context.hh"
#include "http-message.hh"
#include "http-response.hh"
#include "utils/transport/http/nghttp2-client-session.hh"
#include "utils/transport/tls-connection.hh"

namespace flexisip {

/**
 * An HTTTP/2 client over a tls connection.
 * Can be used to established one connection to a remote remote server and send multiple request over this connection.
 * Tls connection and http/2 connection handling is done by the Http2Client.
 */
class Http2Client : public std::enable_shared_from_this<Http2Client> {
public:
	using HttpRequest = HttpMessage;
	using OnErrorCb = HttpMessageContext::OnErrorCb;
	using OnResponseCb = HttpMessageContext::OnResponseCb;

	enum class State : uint8_t { Disconnected, Connected, Connecting };

	class Session : public Nghttp2ClientSession {
	public:
		friend class Http2Client;

		ssize_t onSend(const uint8_t* data, size_t length) noexcept override;
		ssize_t onRecv(uint8_t* buf, size_t length) noexcept override;
		int onFrameSent(const nghttp2_frame& frame) noexcept override;
		int onFrameRecv(const nghttp2_frame& frame) noexcept override;
		int onHeaderRecv(const nghttp2_frame& frame,
		                 std::basic_string_view<uint8_t> name,
		                 std::basic_string_view<uint8_t> value,
		                 uint8_t flags) noexcept override;
		int onDataChunkRecv(uint8_t flags, StreamId, std::basic_string_view<uint8_t> data) noexcept override;
		int onStreamClosed(std::unique_ptr<StreamDataProvider>&&, uint32_t error_code) noexcept override;

		std::string getHost() const {
			return mConn->getPort() == "443" ? mConn->getHost() : mConn->getHost() + ":" + mConn->getPort();
		}

		void enableInsecureTestMode() {
			mConn->enableInsecureTestMode();
		}

		const std::unique_ptr<TlsConnection>& getConnection() const {
			return mConn;
		}

	private:
		std::unique_ptr<TlsConnection> mConn{};
	};

	class BadStateError : public std::logic_error {
	public:
		BadStateError(State state) : logic_error(formatWhatArg(state)) {
		}

	private:
		static std::string formatWhatArg(State state) noexcept;
	};

	template <typename... Args>
	static std::shared_ptr<Http2Client> make(Args&&... args) {
		// new because make_shared need a public constructor.
		return std::shared_ptr<Http2Client>{new Http2Client{std::forward<Args>(args)...}};
	};
	virtual ~Http2Client() = default;

	/**
	 * Send a request to the remote server. OnResponseCb is called if the server return a complete answer. OnErrorCb is
	 * called if any unexpected errors occurred (like connection errors or timeouts).
	 * If an HTTP/2 connection is already active between you and the remote server this connection is re-used. Else a
	 * new connection is automatically created.
	 *
	 * @param request A std::shared_ptr pointing to a HttpMessage object, the message to send.
	 * @param onResponseCb The callback called when a complete answer is received.
	 * @param onErrorCb The callback called when an unexpected error occurred.
	 */
	void
	send(const std::shared_ptr<HttpRequest>& request, const OnResponseCb& onResponseCb, const OnErrorCb& onErrorCb);

	void onTlsConnectCb();

	/**
	 * Test whether the client is processing an HTTP request.
	 * A request is under processing when it has been sent to the HTTP server
	 * or it has been queued until the connection on the server is completed.
	 * @return True when the client isn't processing any request.
	 */
	bool isIdle() const {
		return mActiveHttpContexts.empty() && mPendingHttpContexts.empty();
	}

	/**
	 * Set the request timeout with a new value, but request timeout MUST be inferior to Http2Client::sIdleTimeout to
	 * work properly.
	 * The new timeout is valid only for future requests.
	 */
	Http2Client& setRequestTimeout(std::chrono::seconds requestTimeout) {
		this->mRequestTimeout = requestTimeout;
		return *this;
	}

private:
	// Constructors must be private because Http2Client extends enable_shared_from_this. Use make instead.
	Http2Client(sofiasip::SuRoot& root, std::unique_ptr<TlsConnection>&& connection);
	Http2Client(sofiasip::SuRoot& root, const std::string& host, const std::string& port);
	Http2Client(sofiasip::SuRoot& root,
	            const std::string& host,
	            const std::string& port,
	            const std::string& trustStorePath,
	            const std::string& certPath);

	/* Private methods */
	void sendAllPendingRequests();
	void discardAllPendingRequests();
	void discardAllActiveRequests();

	ssize_t doRecv(nghttp2_session& session, uint8_t* data, size_t length) noexcept;
	void onFrameSent(nghttp2_session& session, const nghttp2_frame& frame) noexcept;
	void onFrameRecv(nghttp2_session& session, const nghttp2_frame& frame) noexcept;
	void onHeaderRecv(nghttp2_session& session,
	                  const nghttp2_frame& frame,
	                  const std::string& name,
	                  const std::string& value,
	                  uint8_t flags) noexcept;
	void onDataReceived(
	    nghttp2_session& session, uint8_t flags, int32_t streamId, const uint8_t* data, size_t datalen) noexcept;
	void onStreamClosed(nghttp2_session& session, int32_t stream_id, uint32_t error_code) noexcept;

	static int onPollInCb(su_root_magic_t*, su_wait_t*, su_wakeup_arg_t* arg) noexcept;

	void resetIdleTimer() noexcept {
		mIdleTimer.set([this]() { onConnectionIdle(); });
	}
	void onConnectionIdle() noexcept;

	void onRequestTimeout(int32_t streamId);
	void resetTimeoutTimer(int32_t streamId);

	void tlsConnect();
	void http2Setup();
	void disconnect();

	void setState(State state) noexcept;

	// Private attributes
	State mState{State::Disconnected};
	sofiasip::SuRoot& mRoot;
	su_wait_t mPollInWait{0};
	sofiasip::Timer mIdleTimer;
	std::string mLogPrefix{};
	int32_t mLastSID{-1};

	std::optional<Session> mHttpSession = std::nullopt;

	using HttpContextList = std::vector<std::shared_ptr<HttpMessageContext>>;
	HttpContextList mPendingHttpContexts{};

	using HttpContextMap = std::map<int32_t, std::shared_ptr<HttpMessageContext>>;
	HttpContextMap mActiveHttpContexts{};

	using TimeoutTimerMap = std::map<int32_t, std::shared_ptr<sofiasip::Timer>>;
	TimeoutTimerMap mTimeoutTimers;

	/**
	 * Delay (in second) for one request timeout, default is 30. Must be inferior to Http2Client::sIdleTimeout.
	 */
	std::chrono::seconds mRequestTimeout{30};

	/**
	 * Delay (in second) before the connection with the distant HTTP2 server is closed because of inactivity.
	 */
	constexpr static std::chrono::seconds mIdleTimeout{60};
};

class Http2Tools {
public:
	static const char* frameTypeToString(uint8_t frameType) noexcept;
	static std::string printFlags(uint8_t flags) noexcept;
};

std::ostream& operator<<(std::ostream& os, const ::nghttp2_frame& frame) noexcept;
std::ostream& operator<<(std::ostream& os, flexisip::Http2Client::State state) noexcept;
} // namespace flexisip
