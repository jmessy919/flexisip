/*
 Flexisip, a flexible SIP proxy server with media capabilities.
 Copyright (C) 2010-2021  Belledonne Communications SARL, All rights reserved.

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

#pragma once

#include <chrono>
#include <functional>
#include <memory>

#include <sofia-sip/su_wait.h>

#include "flexisip/sofia-wrapper/timer.hh"

#include "http-message.hh"
#include "http-response.hh"
#include "utils/transport/http/nghttp2-client-session.hh"

namespace flexisip {

class HttpMessageContext : public Nghttp2ClientSession::StreamDataProvider {
public:
	using HttpRequest = HttpMessage;
	using OnResponseCb = std::function<void(const std::shared_ptr<HttpRequest>&, const std::shared_ptr<HttpResponse>&)>;
	using OnErrorCb = std::function<void(const std::shared_ptr<HttpRequest>&)>;

	HttpMessageContext(const std::shared_ptr<HttpRequest>& request,
	                   const OnResponseCb& onResponseCb,
	                   const OnErrorCb& onErrorCb,
	                   su_root_t& root,
	                   const std::chrono::milliseconds timeout)
	    : mRequest{request}, mResponse{std::make_shared<HttpResponse>()}, mTimeoutTimer{&root, timeout},
	      mOnResponseCb{onResponseCb}, mOnErrorCb{onErrorCb} {};

	const OnErrorCb& getOnErrorCb() const {
		return mOnErrorCb;
	}

	const OnResponseCb& getOnResponseCb() const {
		return mOnResponseCb;
	}

	const std::shared_ptr<HttpRequest>& getRequest() const {
		return mRequest;
	}

	const std::shared_ptr<HttpResponse>& getResponse() const {
		return mResponse;
	}

	const sofiasip::Timer& getTimeoutTimer() const {
		return mTimeoutTimer;
	}

	sofiasip::Timer& getTimeoutTimer() {
		return mTimeoutTimer;
	}

	ssize_t read(uint8_t* buf, size_t length, uint32_t& data_flags) override {
		return mRequest->getDataProvider().read(buf, length, data_flags);
	}

private:
	std::shared_ptr<HttpRequest> mRequest;
	std::shared_ptr<HttpResponse> mResponse;
	sofiasip::Timer mTimeoutTimer;
	OnResponseCb mOnResponseCb;
	OnErrorCb mOnErrorCb;
};

} /* namespace flexisip */
