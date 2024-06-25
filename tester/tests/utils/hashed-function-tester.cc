/*
    Flexisip, a flexible SIP proxy server with media capabilities.
    Copyright (C) 2010-2024 Belledonne Communications SARL, All rights reserved.

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

#include "utils/hashed-function.hh"

#include "utils/test-patterns/test.hh"
#include "utils/test-suite.hh"

using namespace std;

namespace {

const auto& anonymousLambda = [fillerData = ""]() { std::ignore = fillerData; };

class HashableFunctor {
public:
	void operator()() {
	}
};

} // namespace

namespace std {

template <>
struct hash<HashableFunctor> {
	size_t operator()(const HashableFunctor&) const {
		return 42;
	}
};

template <>
struct hash<decay_t<decltype(anonymousLambda)>> {
	size_t operator()(decltype(anonymousLambda) obj) const {
		BC_ASSERT_CPP_EQUAL(std::string_view(reinterpret_cast<const char*>(std::addressof(obj)), sizeof(decltype(obj))),
		                    std::string_view(reinterpret_cast<const char*>(std::addressof(anonymousLambda)),
		                                     sizeof(decltype(anonymousLambda))));
		return 55;
	}
};

} // namespace std

namespace flexisip::tester {

namespace {
using namespace utils;

void customHasher() {
	BC_ASSERT_CPP_EQUAL(HashedFunction<void()>(HashableFunctor()).hash, 42);
	BC_ASSERT_CPP_EQUAL(HashedFunction<void()>(anonymousLambda).hash, 55);
	BC_ASSERT_CPP_EQUAL(HashedFunction<void()>([]() {},
	                                           [](const auto& obj) {
		                                           BC_ASSERT_CPP_NOT_EQUAL(std::addressof(obj), nullptr);
		                                           return 914;
	                                           })
	                        .hash,
	                    914);
}

TestSuite _("utils::HashedFunction",
            {
                CLASSY_TEST(customHasher),
            });
} // namespace

} // namespace flexisip::tester
