LOCAL_PATH := $(call my-dir)
ifeq ($(OVR_DEBUG),1)
  BUILDTYPE := Debug
else
 BUILDTYPE := Release
endif

#--------------------------------------------------------
# libvrapi.so
#
# VrApi
#--------------------------------------------------------
include $(CLEAR_VARS)

LOCAL_MODULE := vrapi

LOCAL_SRC_FILES := ../../../Libs/Android/$(TARGET_ARCH_ABI)/$(BUILDTYPE)/lib$(LOCAL_MODULE).so

# only export public headers
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../../Include

# NOTE: This check is added to prevent the following error when running a "make clean" where
# the prebuilt lib may have been deleted: "LOCAL_SRC_FILES points to a missing file"
ifneq (,$(wildcard $(LOCAL_PATH)/$(LOCAL_SRC_FILES)))
  include $(PREBUILT_SHARED_LIBRARY)
endif
