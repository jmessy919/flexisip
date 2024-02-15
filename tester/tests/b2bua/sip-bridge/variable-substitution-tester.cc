/** Copyright (C) 2010-2024 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "b2bua/sip-bridge/variable-substitution.hh"

#include "utils/string-interpolation/preprocessed-interpolated-string.hh"
#include "utils/test-patterns/test.hh"
#include "utils/test-suite.hh"

namespace flexisip::tester {
namespace {
using namespace utils::string_interpolation;
using namespace b2bua::bridge::variable_substitution;

bool operator==(const StringViewMold& left, const StringViewMold& right) {
	return left.start == right.start && left.size == right.size;
}

std::ostream& operator<<(std::ostream& ostr, const StringViewMold& mold) {
	return ostr << "StringViewMold{ .start = " << mold.start << ", .size = " << mold.size << "}";
}

void tryParse(std::string template_) {
	PreprocessedInterpolatedString<const linphone::Call&>(
	    InterpolatedString(template_, "{", "}"),
	    resolve([](const auto& call) -> const linphone::Call& { return call; }, linphone_call::kFields));
}

std::size_t charCount(std::string_view view) {
	return view.size();
}

void knownFields() {
	tryParse("{to.hostport}");
}

void unknownFields() {
	try {
		tryParse("{unknown.hostport}");
		BC_FAIL("expected exception");
	} catch (const ResolutionError& err) {
		const auto& expected = StringViewMold{.start = charCount("{"), .size = charCount("unknown")};
		BC_ASSERT_CPP_EQUAL(err.offendingToken, expected);
	}

	try {
		tryParse("{to.hostport.what}");
		BC_FAIL("expected exception");
	} catch (const ResolutionError& err) {
		const auto& expected = StringViewMold{.start = charCount("{to.hostport."), .size = charCount("what")};
		BC_ASSERT_CPP_EQUAL(err.offendingToken, expected);
	}
}

// TODO: test missing closing delim
// TODO: test errors at format time. E.g. empty SipUri.

TestSuite _{
    "b2bua::bridge::variable_substitution",
    {
        CLASSY_TEST(knownFields),
        CLASSY_TEST(unknownFields),
    },
};
} // namespace
} // namespace flexisip::tester