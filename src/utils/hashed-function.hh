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

#pragma once

#include <functional>

namespace flexisip::utils {

/* `std::function` wrapper that exposes the hash of the target type
 *
 * The hash is eagerly computed
 */
template <typename... Signature>
class HashedFunction : public std::function<Signature...> {
public:
	template <typename F, typename Hash = std::hash<std::decay_t<F>>>
	HashedFunction(F&& f, Hash hasher = std::hash<std::decay_t<F>>())
	    : std::function<Signature...>(std::forward<F>(f)), hash(hasher(*this->template target<std::decay_t<F>>())) {
	}

	const std::size_t hash;
};

} // namespace flexisip::utils

namespace std {

template <typename... Args>
struct hash<flexisip::utils::HashedFunction<Args...>> {
	size_t operator()(const flexisip::utils::HashedFunction<Args...>& hashedFunc) const {
		return hashedFunc.hash;
	}
};

} // namespace std
