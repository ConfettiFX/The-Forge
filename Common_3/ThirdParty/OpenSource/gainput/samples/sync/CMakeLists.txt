
project(syncsample)

include_directories(../../lib/include/)
include_directories(../samplefw/)

file(GLOB_RECURSE sources *.cpp)
file(GLOB_RECURSE sfwsources ../samplefw/*.cpp)

if(ANDROID)
	add_library(syncsample SHARED ${sources} ${sfwsources})
else()
	add_executable(syncsample WIN32 ${sources} ${sfwsources})
endif()

target_link_libraries(syncsample gainputstatic)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	target_link_libraries(syncsample X11 GL rt)
elseif(WIN32)
	target_link_libraries(syncsample ${XINPUT} ws2_32)
elseif(APPLE)
	target_link_libraries(syncsample ${FOUNDATION} ${IOKIT} ${APPKIT})
endif()
