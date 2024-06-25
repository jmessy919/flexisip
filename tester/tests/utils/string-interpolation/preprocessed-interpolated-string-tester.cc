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

#include "utils/string-interpolation/preprocessed-interpolated-string.hh"

#include "b2bua/sip-bridge/variable-substitution.hh"
#include "utils/test-patterns/test.hh"
#include "utils/test-suite.hh"

using namespace std;

namespace flexisip::tester {

namespace {
using namespace b2bua::bridge::variable_substitution;
using utils::string_interpolation::InterpolatedString;
using PIS = utils::string_interpolation::PreprocessedInterpolatedString<int>;

void hashEquality() {
	constexpr auto buildAndHash = [](const auto& resolver) {
		return std::hash<PIS>()(PIS(InterpolatedString("stub {template}", "{", "}"), resolver));
	};

	const auto& lambdaResolver =
	    PIS::Resolver([](const auto&) { return [](const auto&) { return ""s; }; }, [](const auto&) { return 0xc0de; });
	BC_ASSERT_CPP_EQUAL(buildAndHash(lambdaResolver), buildAndHash(lambdaResolver));

	BC_ASSERT_CPP_EQUAL(buildAndHash(lambdaResolver),
	                    buildAndHash(PIS::Resolver([](const auto&) { return [](const auto&) { return ""s; }; },
	                                               [](const auto&) { return 0xc0de; })));

	const auto& stubFields = FieldsOf<int>{
	    {"template", leaf([](const auto&) { return ""s; })},
	};
	BC_ASSERT_CPP_EQUAL(typeid(FieldsResolver{stubFields}).hash_code(), typeid(FieldsResolver{stubFields}).hash_code());
	BC_ASSERT_CPP_EQUAL(buildAndHash(FieldsResolver{stubFields}), buildAndHash(FieldsResolver{stubFields}));

	const auto& otherFields = FieldsOf<int>{
	    {"template", leaf([](const auto&) { return "something else"s; })},
	};
	BC_ASSERT_CPP_EQUAL(typeid(FieldsResolver{stubFields}).hash_code(),
	                    typeid(FieldsResolver{otherFields}).hash_code());
	BC_ASSERT_CPP_EQUAL(typeid(FieldsResolver{stubFields}("template")).hash_code(),
	                    typeid(FieldsResolver{otherFields}("template")).hash_code());
	BC_ASSERT_CPP_NOT_EQUAL(buildAndHash(FieldsResolver{stubFields}), buildAndHash(FieldsResolver{otherFields}));
}

TestSuite _("utils::string_interpolation::PreprocessedInterpolatedString",
            {
                CLASSY_TEST(hashEquality),
            });
} // namespace

} // namespace flexisip::tester
