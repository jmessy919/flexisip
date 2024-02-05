/** Copyright (C) 2010-2024 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "exceptions.hh"
#include "utils/string-interpolation/string-view-mold.hh"

namespace flexisip::utils::string_interpolation {

class InterpolatedString {
public:
	struct Members {
		std::string templateString{};
		std::vector<StringViewMold> pieces{};
		std::vector<StringViewMold> symbols{};
	};

	class MissingClosingDelimiter : public ParseError {
	public:
		MissingClosingDelimiter() : ParseError("invalid template: missing closing delimiter") {
		}
	};

	/**
	 * @throws MissingClosingDelimiter
	 */
	explicit InterpolatedString(std::string templateString, std::string_view startDelim, std::string_view endDelim);

	Members&& extractMembers() && {
		return std::move(m);
	}

private:
	Members m{};
};

} // namespace flexisip::utils::string_interpolation
