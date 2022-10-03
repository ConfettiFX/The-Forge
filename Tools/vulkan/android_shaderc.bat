echo off
setlocal

set ndk_root=%1

REM Internally, shaderc makefiles try to change directory. They aren't set up
REM properly for Windows, and will fail if the android SDK and your current
REM working directory are on different drives.
CD /D %ndk_root:~0,3%

%ndk_root%ndk-build NDK_PROJECT_PATH=%ndk_root%sources\third_party\shaderc APP_BUILD_SCRIPT=%ndk_root%sources\third_party\shaderc\Android.mk APP_STL:=c++_shared APP_ABI=arm64-v8a libshaderc_combined APP_PLATFORM=android-28 -j16

endlocal