/** Copyright (C) 2010-2024 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "interpolated-string.hh"

namespace flexisip::utils::string_interpolation {

InterpolatedString::InterpolatedString(std::string templateString,
                                       std::string_view startDelim,
                                       std::string_view endDelim) {
	std::size_t currentIndex(0);
	while (true) {
		const auto startIndex = templateString.find(startDelim, currentIndex);
		m.pieces.emplace_back(StringViewMold{.start = currentIndex, .size = startIndex - currentIndex});
		if (startIndex == std::string_view::npos) break;

		currentIndex = startIndex + startDelim.size();
		const auto endIndex = templateString.find(endDelim, currentIndex);
		if (endIndex == std::string_view::npos) {
			throw MissingClosingDelimiter(templateString, endDelim, startIndex);
		}

		m.symbols.emplace_back(StringViewMold{.start = currentIndex, .size = endIndex - currentIndex});
		currentIndex = endIndex + endDelim.size();
	}

	m.templateString = std::move(templateString);
}

std::string InterpolatedString::canonical() const {
	auto canonical = std::string(m.templateString);
	for (const auto [start, size] : m.symbols) {
		canonical[start - 1] = canonical[start + size] = '\0';
	}

	return canonical;
}

} // namespace flexisip::utils::string_interpolation

namespace std {
using flexisip::utils::string_interpolation::InterpolatedString;

size_t hash<InterpolatedString>::operator()(const InterpolatedString& interpolated) const {
	return hash<string>()(interpolated.canonical());
}

} // namespace std
