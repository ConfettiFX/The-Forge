
project(dynamicsample)

include_directories(../../lib/include/)
include_directories(../samplefw/)

file(GLOB_RECURSE sources *.cpp)
file(GLOB_RECURSE sfwsources ../samplefw/*.cpp)

if(ANDROID)
	add_library(dynamicsample SHARED ${sources} ${sfwsources})
else()
	add_executable(dynamicsample WIN32 ${sources} ${sfwsources})
endif()

target_link_libraries(dynamicsample gainputstatic)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	target_link_libraries(dynamicsample X11 GL rt)
elseif(WIN32)
	target_link_libraries(dynamicsample ${XINPUT} ws2_32)
elseif(APPLE)
	target_link_libraries(dynamicsample ${FOUNDATION} ${IOKIT} ${APPKIT})
endif()

