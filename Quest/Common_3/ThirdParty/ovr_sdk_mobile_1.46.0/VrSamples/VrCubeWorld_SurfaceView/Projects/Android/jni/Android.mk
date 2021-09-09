LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := vrcubeworldsv


LOCAL_CFLAGS += -std=c99 -Werror

LOCAL_SRC_FILES := ../../../Src/VrCubeWorld_SurfaceView.c

LOCAL_LDLIBS := -lEGL -lGLESv3 -landroid -llog




LOCAL_SHARED_LIBRARIES := vrapi

include $(BUILD_SHARED_LIBRARY)

$(call import-module,VrApi/Projects/AndroidPrebuilt/jni)
