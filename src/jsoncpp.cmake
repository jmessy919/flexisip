# SPDX-License-Identifier: GPL-3.0-or-later

if(INTERNAL_JSONCPP)
	if(NOT TARGET jsoncpp_lib)
		add_subdirectory(../linphone-sdk/external/jsoncpp ${CMAKE_CURRENT_BINARY_DIR}/jsoncpp)
	endif()
else()
	find_package(jsoncpp)
	if(NOT jsoncpp_FOUND)
		message(FATAL_ERROR "Could NOT find jsoncpp. If your system cannot provide it, try to build the vendored version with -DINTERNAL_JSONCPP=YES.")
	endif()
endif()
