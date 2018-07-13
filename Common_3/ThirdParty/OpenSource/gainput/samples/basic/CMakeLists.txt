
project(basicsample)

include_directories(../../lib/include/)

file(GLOB_RECURSE sources *.cpp)

if(APPLE)
    file(GLOB_RECURSE mmsources *.mm)
endif()

if(ANDROID)
	add_library(basicsample SHARED ${sources})
elseif(APPLE AND IOS)
  set(imagessources
    "${CMAKE_CURRENT_LIST_DIR}/../../extern/ios/Launch Screen.storyboard"
  )
  add_executable(basicsample "MACOSX_BUNDLE" ${sources} ${mmsources} ${imagessources})
  set_target_properties( basicsample PROPERTIES
    XCODE_ATTRIBUTE_INFOPLIST_FILE "${CMAKE_CURRENT_LIST_DIR}/../../extern/ios/Info.plist"
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "iPhone Developer"
  )
  target_link_libraries(basicsample ${UIKIT} ${QUARTZCORE})
else()
	add_executable(basicsample WIN32 ${sources} ${mmsources})
endif()

target_link_libraries(basicsample gainputstatic)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	target_link_libraries(basicsample X11 GL rt)
elseif(WIN32)
	target_link_libraries(basicsample ${XINPUT} ws2_32)
elseif(APPLE)
    target_link_libraries(basicsample ${FOUNDATION} ${IOKIT} ${APPKIT} ${GAME_CONTROLLER})
endif()

