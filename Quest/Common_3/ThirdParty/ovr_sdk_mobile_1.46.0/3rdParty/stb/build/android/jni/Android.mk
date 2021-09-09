LOCAL_PATH := $(call my-dir)/../../..

include $(CLEAR_VARS)

LOCAL_MODULE := stb
LOCAL_ARM_MODE := arm
LOCAL_ARM_NEON := true

include $(LOCAL_PATH)/../../cflags.mk
LOCAL_CFLAGS   := -w
LOCAL_CPPFLAGS := -w

LOCAL_SRC_FILES := \
  src/stb_image.c \
  src/stb_image_write.c \
  src/stb_vorbis.c

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/src

include $(BUILD_STATIC_LIBRARY)
