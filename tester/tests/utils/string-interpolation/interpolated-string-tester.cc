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

#include "utils/string-interpolation/interpolated-string.hh"

#include "utils/test-patterns/test.hh"
#include "utils/test-suite.hh"

using namespace std;

namespace flexisip::tester {

namespace {
using utils::string_interpolation::InterpolatedString;

void canonicalForm() {
	const auto& canonical =
	    InterpolatedString(
	        "In its canonical form an interpolated string has all its {delimiters} replaced with null chars", "{", "}")
	        .canonical();

	BC_ASSERT_CPP_EQUAL(
	    canonical, "In its canonical form an interpolated string has all its \0delimiters\0 replaced with null chars"s);
	BC_ASSERT_CPP_NOT_EQUAL(
	    canonical, "In its canonical form an interpolated string has all its \0delimiters\0"s);
	BC_ASSERT_CPP_NOT_EQUAL(
	    canonical, "In its canonical form an interpolated string has all its \0"s);
	BC_ASSERT_CPP_NOT_EQUAL(
	    canonical, "In its canonical form an interpolated string has all its "s);
}

void hashEquality() {
	constexpr auto hasher = std::hash<InterpolatedString>();

	BC_ASSERT_CPP_EQUAL(
	    hasher(InterpolatedString("These two templates hash to the same value regardless of {delimiters}", "{", "}")),
	    hasher(InterpolatedString("These two templates hash to the same value regardless of /delimiters/", "/", "/")));

	BC_ASSERT_CPP_NOT_EQUAL(hasher(InterpolatedString("Same {pieces} and {symbols}, different {order}", "{", "}")),
	                        hasher(InterpolatedString("Same {symbols} and {pieces}, different {order}", "{", "}")));
}

TestSuite _("utils::string_interpolation::InterpolatedString",
            {
                CLASSY_TEST(canonicalForm),
                CLASSY_TEST(hashEquality),
            });
} // namespace

} // namespace flexisip::tester
