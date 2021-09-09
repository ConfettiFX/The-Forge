LOCAL_PATH := $(call my-dir)/../../..

include $(CLEAR_VARS)

LOCAL_MODULE := minizip
LOCAL_ARM_MODE := arm
LOCAL_ARM_NEON := true

include $(LOCAL_PATH)/../../cflags.mk

LOCAL_SRC_FILES := \
  src/ioapi.c \
  src/miniunz.c \
  src/mztools.c \
  src/unzip.c \
  src/zip.c
  
  	
LOCAL_CFLAGS	+= -Wno-shadow

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/src

LOCAL_EXPORT_LDLIBS += -lz

include $(BUILD_STATIC_LIBRARY)
