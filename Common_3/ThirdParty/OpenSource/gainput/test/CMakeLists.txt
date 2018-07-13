
project(gainputtest)

include_directories(../lib/include/)
include_directories(../extern/catch/)

file(GLOB_RECURSE sources *.cpp)

if(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
endif()

if(ANDROID)
	add_library(gainputtest SHARED ${sources})
else()
	add_executable(gainputtest WIN32 ${sources})
endif()

if(APPLE AND NOT IOS)
	find_library(APPKIT AppKit)
else()
	set(APPKIT "")
endif()

target_link_libraries(gainputtest gainputstatic)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	target_link_libraries(gainputtest X11 GL rt)
elseif(WIN32)
	target_link_libraries(gainputtest ${XINPUT} ws2_32)
elseif(APPLE)
	target_link_libraries(gainputtest ${FOUNDATION} ${IOKIT} ${APPKIT})
endif()

if(MSVC)
	set_target_properties(gainputtest PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
endif(MSVC)
