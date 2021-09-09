# Common build settings for all VR apps
# NOTE: These properties are ignored for any libraries or applications
# using the externalNativeBuild path. This file can go away, once
# remaining usage can be stripped from test libraries and applications.

# This needs to be defined to get the right header directories for egl / etc
# NOTE: this is ignored from here now, and must be specified in build.gradle!
APP_PLATFORM := android-24

# Statically link the C++_STATIC STL. This may not be safe for multi-so libraries but
# we don't know of any problems yet.
APP_STL := c++_static

# Make sure every shared lib includes a .note.gnu.build-id header, for crash reporting
APP_LDFLAGS := -Wl,--build-id

NDK_TOOLCHAIN_VERSION := clang

# Define the directories for $(import-module, ...) to look in
ROOT_DIR := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
NDK_MODULE_PATH := $(ROOT_DIR)

# ndk-r14 introduced failure for missing dependencies. If 'false', the clean
# step will error as we currently remove prebuilt artifacts on clean.
APP_ALLOW_MISSING_DEPS=true
