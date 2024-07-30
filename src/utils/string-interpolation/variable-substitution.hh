/** Copyright (C) 2010-2024 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <string>

#include "utils/string-interpolation/exceptions.hh"
#include "utils/string-utils.hh"

namespace flexisip::utils::string_interpolation {

// TODO DOC
template <typename... Args>
using Substituter = std::function<std::string(const Args&...)>;

// TODO DOC
template <typename... Args>
using Resolver = std::function<Substituter<Args...>(std::string_view)>;

// TODO DOC
template <typename... Args>
using FieldsOf = std::unordered_map<std::string_view, Resolver<Args...>>;

/**
 * @brief Builds a leaf Resolver that does not accept any sub fields
 *
 * @param substituter the substitution function for this field
 */
template <typename TSubstituter>
constexpr auto leaf(TSubstituter substituter) {
	return [substituter](std::string_view furtherPath) {
		if (furtherPath != "") {
			throw utils::string_interpolation::ContextlessResolutionError(furtherPath);
		}

		return substituter;
	};
}

// TODO DOC
inline std::pair<std::string_view, std::string_view> popVarName(std::string_view dotPath) {
	const auto split = StringUtils::splitOnce(dotPath, ".");
	if (!split) return {dotPath, ""};

	const auto [head, tail] = *split;
	return {head, tail};
}

/**
 * @brief Builds a (sub-)Resolver from a transformation function and fields map
 *
 * @param fields Available fields in this resolution context
 * @param transformer Callable to extract a new sub-context (field) from the current context
 */
template <typename... Context, typename Transformer = std::nullopt_t>
constexpr auto resolve(FieldsOf<Context...> const& fields, Transformer transformer = std::nullopt) {
	return [transformer, &fields](const auto dotPath) {
		const auto& [varName, furtherPath] = popVarName(dotPath);
		const auto& resolver = fields.find(varName);
		if (resolver == fields.end()) {
			throw utils::string_interpolation::ContextlessResolutionError(varName);
		}

		const auto& substituter = resolver->second(furtherPath);

		return [substituter, transformer](const auto&... args) {
			if constexpr (!std::is_same_v<Transformer, std::nullopt_t>) {
				return substituter(transformer(args...));
			} else {
				return substituter(args...);
				std::ignore = transformer; // Suppress unused warning
			}
		};
	};
}

} // namespace flexisip::utils::string_interpolation