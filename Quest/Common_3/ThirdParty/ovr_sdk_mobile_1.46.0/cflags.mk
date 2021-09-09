# This file is included in all .mk files to ensure their compilation flags are in sync
# across debug and release builds.
# NOTE: This file will not get translated to BUCK. For enabling ASAN on buck, update build_defs/mobile_oxx.bzl
ENABLE_SANITIZER := 0

LOCAL_CFLAGS	:= -DANDROID_NDK
LOCAL_CFLAGS	+= -Werror			# error on warnings
LOCAL_CFLAGS	+= -Wall
LOCAL_CFLAGS	+= -Wextra
LOCAL_CFLAGS	+= -Wshadow
#LOCAL_CFLAGS	+= -Wlogical-op		# not part of -Wall or -Wextra
#LOCAL_CFLAGS	+= -Weffc++			# too many issues to fix for now
LOCAL_CFLAGS	+= -Wno-unused-parameter
LOCAL_CFLAGS	+= -Wno-missing-field-initializers	# warns on this: SwipeAction	ret = {}
LOCAL_CPPFLAGS += -std=c++17

ifeq ($(OVR_DEBUG),1)
  LOCAL_CFLAGS += -DOVR_BUILD_DEBUG=1 -O0 -g

  ifeq ($(ENABLE_SANITIZER),1)
    $(info "-----------ENABLE_SANITIZER-----------")
    ifeq ($(USE_ASAN),1)
      LOCAL_CFLAGS += -fsanitize=address -fno-omit-frame-pointer
      LOCAL_CPPFLAGS += -fsanitize=address -fno-omit-frame-pointer
      LOCAL_LDFLAGS += -fsanitize=address
    endif
  endif
else
  LOCAL_CFLAGS += -O3
endif

# Explicitly compile for the ARM and not the Thumb instruction set.
LOCAL_ARM_MODE := arm
