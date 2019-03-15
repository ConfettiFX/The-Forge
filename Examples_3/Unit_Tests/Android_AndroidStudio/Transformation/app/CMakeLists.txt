cmake_minimum_required(VERSION 3.4.1)

# build native_app_glue as a static lib
set(SHADERC_DIR ${ANDROID_NDK}/sources/third_party/shaderc)
add_library (shaderc STATIC IMPORTED)
set_target_properties(shaderc PROPERTIES
  IMPORTED_LOCATION ${SHADERC_DIR}/libs/c++_static/${ANDROID_ABI}/libshaderc.a)

# build native_app_glue as a static lib
set(APP_GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)
include_directories(${APP_GLUE_DIR})
add_library( app-glue STATIC ${APP_GLUE_DIR}/android_native_app_glue.c)

set(COMMON_DIR ../../../../../Common_3)
set(MIDDLEWARE_DIR ../../../../../Middleware_3)
add_library(os STATIC
            # core common
            ${COMMON_DIR}/OS/Android/AndroidFileSystem.cpp
            ${COMMON_DIR}/OS/Android/AndroidBase.cpp
            ${COMMON_DIR}/OS/Android/AndroidLogManager.cpp
            ${COMMON_DIR}/OS/Android/AndroidThreadManager.cpp
            ${COMMON_DIR}/OS/Camera/FpsCameraController.cpp
            ${COMMON_DIR}/OS/Camera/GuiCameraController.cpp            
            ${COMMON_DIR}/OS/Core/FileSystem.cpp
            ${COMMON_DIR}/OS/Core/PlatformEvents.cpp
            ${COMMON_DIR}/OS/Core/ThreadSystem.cpp
            ${COMMON_DIR}/OS/Core/Timer.cpp
            ${COMMON_DIR}/OS/Core/Timer.cpp
            ${COMMON_DIR}/OS/Image/Image.cpp
            ${COMMON_DIR}/OS/Logging/LogManager.cpp
            ${COMMON_DIR}/OS/Image/Image.cpp
            ${COMMON_DIR}/OS/Input/InputSystem.cpp
            ${COMMON_DIR}/OS/MemoryTracking/MemoryTrackingManager.cpp
            ${COMMON_DIR}/ThirdParty/OpenSource/TinyEXR/tinyexr.cpp

            # middleware
            ${MIDDLEWARE_DIR}/Text/Fontstash.cpp
            ${MIDDLEWARE_DIR}/UI/AppUI.cpp
            ${MIDDLEWARE_DIR}/UI/ImguiGUIDriver.cpp

            # imgui
            ${COMMON_DIR}/ThirdParty/OpenSource/imgui/imgui.cpp
            ${COMMON_DIR}/ThirdParty/OpenSource/imgui/imgui_demo.cpp
            ${COMMON_DIR}/ThirdParty/OpenSource/imgui/imgui_draw.cpp
            ${COMMON_DIR}/ThirdParty/OpenSource/imgui/imgui_widgets.cpp
            )
target_compile_definitions(os PUBLIC VULKAN)
target_link_libraries(os shaderc)

add_library(renderer STATIC
            # vulkan renderer
            ${COMMON_DIR}/Renderer/Vulkan/Vulkan.cpp
            ${COMMON_DIR}/Renderer/Vulkan/VulkanRaytracing.cpp
            ${COMMON_DIR}/Renderer/Vulkan/VulkanShaderReflection.cpp
            ${COMMON_DIR}/Renderer/CommonShaderReflection.cpp
            ${COMMON_DIR}/Renderer/GpuProfiler.cpp
            ${COMMON_DIR}/Renderer/ResourceLoader.cpp

            # spirv tools
            ${COMMON_DIR}/ThirdParty/OpenSource/SPIRV_Cross/spirv_cfg.cpp
            ${COMMON_DIR}/ThirdParty/OpenSource/SPIRV_Cross/spirv_cross.cpp
            ${COMMON_DIR}/Tools/SpirvTools/SpirvTools.cpp
            )
target_compile_definitions(renderer PUBLIC VULKAN)
target_include_directories(renderer PUBLIC ${SHADERC_DIR}/include)
target_link_libraries(renderer os shaderc)

# build ga input
set(GAINPUT_DIR ../../../../../Common_3/ThirdParty/OpenSource/gainput/lib/source/gainput)
set(GAINPUTINCLUDE_DIR ../../../../../Common_3/ThirdParty/OpenSource/gainput/lib/include)

add_library(gainput STATIC
            ${GAINPUT_DIR}/gainput.cpp
            ${GAINPUT_DIR}/GainputAllocator.cpp
            ${GAINPUT_DIR}/GainputInputDeltaState.cpp
            ${GAINPUT_DIR}/GainputInputDevice.cpp
            ${GAINPUT_DIR}/GainputInputManager.cpp
            ${GAINPUT_DIR}/GainputInputMap.cpp
            ${GAINPUT_DIR}/GainputInputState.cpp
            ${GAINPUT_DIR}/GainputMapFilters.cpp
            # builtin
            ${GAINPUT_DIR}/builtin/GainputInputDeviceBuiltIn.cpp
            # dev
            ${GAINPUT_DIR}/dev/GainputDev.cpp
            ${GAINPUT_DIR}/dev/GainputMemoryStream.cpp
            ${GAINPUT_DIR}/dev/GainputNetAddress.cpp
            ${GAINPUT_DIR}/dev/GainputNetConnection.cpp
            ${GAINPUT_DIR}/dev/GainputNetListener.cpp
            # gesture
            ${GAINPUT_DIR}/gestures/GainputButtonStickGesture.cpp
            ${GAINPUT_DIR}/gestures/GainputDoubleClickGesture.cpp
            ${GAINPUT_DIR}/gestures/GainputHoldGesture.cpp
            ${GAINPUT_DIR}/gestures/GainputPinchGesture.cpp
            ${GAINPUT_DIR}/gestures/GainputRotateGesture.cpp
            ${GAINPUT_DIR}/gestures/GainputSimultaneouslyDownGesture.cpp
            ${GAINPUT_DIR}/gestures/GainputTapGesture.cpp
            # keyboard
            ${GAINPUT_DIR}/keyboard/GainputInputDeviceKeyboard.cpp
            ${GAINPUT_DIR}/keyboard/GainputInputDeviceKeyboardMac.cpp
            # mouse
            ${GAINPUT_DIR}/mouse/GainputInputDeviceMouse.cpp
            # pad
            ${GAINPUT_DIR}/pad/GainputInputDevicePad.cpp
            ${GAINPUT_DIR}/pad/GainputInputDevicePadMac.cpp
            # recorder
            ${GAINPUT_DIR}/recorder/GainputInputPlayer.cpp
            ${GAINPUT_DIR}/recorder/GainputInputRecorder.cpp
            ${GAINPUT_DIR}/recorder/GainputInputRecording.cpp
            # touch
            ${GAINPUT_DIR}/touch/GainputInputDeviceTouch.cpp
            )
target_compile_definitions(gainput PUBLIC)
target_include_directories(gainput PUBLIC ${GAINPUTINCLUDE_DIR})

# build vulkan app
set(SRC_DIR ../../../src)
set(APP_NAME 01_Transformations)
add_library(transformation SHARED
            ${SRC_DIR}/${APP_NAME}/01_Transformations.cpp
            )

target_compile_definitions(transformation PUBLIC VULKAN)
# ignore these warnings
# unused private field
# unused variable
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Werror -Wno-unused-private-field -Wno-unused-variable -Wno-unused-function\
                     -DVK_USE_PLATFORM_ANDROID_KHR")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

target_include_directories(transformation PUBLIC ${COMMON_DIR}/OS/Interfaces)
target_link_libraries(transformation app-glue shaderc os renderer log vulkan android gainput)



