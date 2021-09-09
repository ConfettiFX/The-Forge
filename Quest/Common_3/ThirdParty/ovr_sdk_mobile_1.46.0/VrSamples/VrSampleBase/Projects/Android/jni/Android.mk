LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

include ../../../../cflags.mk

LOCAL_MODULE := vrsamplebase


LOCAL_C_INCLUDES :=	$(LOCAL_PATH)/../../../../SampleCommon/Src \
    $(LOCAL_PATH)/../../../../SampleFramework/Src \
    $(LOCAL_PATH)/../../../../../1stParty/OVR/Include \
    $(LOCAL_PATH)/../../../../../1stParty/utilities/include \
    $(LOCAL_PATH)/../../../../../3rdParty/stb/src \

LOCAL_SRC_FILES := ../../../Src/main.cpp

LOCAL_LDLIBS := -lEGL -lGLESv3 -landroid -llog



LOCAL_STATIC_LIBRARIES := sampleframework
LOCAL_SHARED_LIBRARIES := vrapi

include $(BUILD_SHARED_LIBRARY)

$(call import-module,VrApi/Projects/AndroidPrebuilt/jni)
$(call import-module,VrSamples/SampleFramework/Projects/Android/jni)
