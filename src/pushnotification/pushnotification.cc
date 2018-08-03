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

#include <sstream>
#include "pushnotification.hh"



PushNotificationRequest::PushNotificationRequest(const std::string &appid, const std::string &type)
			: mState( NotSubmitted), mAppId(appid), mType(type) {
}

std::string PushNotificationRequest::quoteStringIfNeeded(const std::string &str){
	if (str[0] == '"'){
		return str;
	}else{
		std::ostringstream ostr;
		ostr << "\"" << str << "\"";
		return ostr.str();
	}
}