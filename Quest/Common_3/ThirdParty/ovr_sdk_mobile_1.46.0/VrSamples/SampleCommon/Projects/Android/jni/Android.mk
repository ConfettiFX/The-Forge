LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include ../../../../cflags.mk

LOCAL_MODULE := samplecommon

# full speed arm instead of thumb
LOCAL_ARM_MODE := arm
# compile with neon support enabled
LOCAL_ARM_NEON := true

LOCAL_C_INCLUDES := \
  $(LOCAL_PATH)/../../../Src \
  $(LOCAL_PATH)/../../../../../1stParty/OVR/Include \
  $(LOCAL_PATH)/../../../../../1stParty/utilities/include \
  $(LOCAL_PATH)/../../../../../3rdParty/stb/src \

LOCAL_CFLAGS += -Wno-invalid-offsetof

LOCAL_SRC_FILES := \
  ../../../Src/GUI/ActionComponents.cpp \
  ../../../Src/GUI/AnimComponents.cpp \
  ../../../Src/GUI/CollisionPrimitive.cpp \
  ../../../Src/GUI/DefaultComponent.cpp \
  ../../../Src/GUI/Fader.cpp \
  ../../../Src/GUI/GazeCursor.cpp \
  ../../../Src/GUI/GuiSys.cpp \
  ../../../Src/GUI/MetaDataManager.cpp \
  ../../../Src/GUI/Reflection.cpp \
  ../../../Src/GUI/ReflectionData.cpp \
  ../../../Src/GUI/SoundLimiter.cpp \
  ../../../Src/GUI/VRMenu.cpp \
  ../../../Src/GUI/VRMenuComponent.cpp \
  ../../../Src/GUI/VRMenuEvent.cpp \
  ../../../Src/GUI/VRMenuEventHandler.cpp \
  ../../../Src/GUI/VRMenuMgr.cpp \
  ../../../Src/GUI/VRMenuObject.cpp \
  ../../../Src/Input/ArmModel.cpp \
  ../../../Src/Input/AxisRenderer.cpp \
  ../../../Src/Input/ControllerRenderer.cpp \
  ../../../Src/Input/Skeleton.cpp \
  ../../../Src/Input/SkeletonRenderer.cpp \
  ../../../Src/Input/TinyUI.cpp \
  ../../../Src/Locale/OVR_Locale.cpp \
  ../../../Src/Locale/tinyxml2.cpp \
  ../../../Src/Misc/Log.c \
  ../../../Src/Model/ModelCollision.cpp \
  ../../../Src/Model/ModelFile_glTF.cpp \
  ../../../Src/Model/ModelFile_OvrScene.cpp \
  ../../../Src/Model/ModelFile.cpp \
  ../../../Src/Model/ModelRender.cpp \
  ../../../Src/Model/ModelTrace.cpp \
  ../../../Src/Model/SceneView.cpp \
  ../../../Src/OVR_BinaryFile2.cpp \
  ../../../Src/OVR_FileSys.cpp \
  ../../../Src/OVR_Lexer2.cpp \
  ../../../Src/OVR_MappedFile.cpp \
  ../../../Src/OVR_Stream.cpp \
  ../../../Src/OVR_Uri.cpp \
  ../../../Src/OVR_UTF8Util.cpp \
  ../../../Src/PackageFiles.cpp \
  ../../../Src/Render/BeamRenderer.cpp \
  ../../../Src/Render/BitmapFont.cpp \
  ../../../Src/Render/DebugLines.cpp \
  ../../../Src/Render/EaseFunctions.cpp \
  ../../../Src/Render/Egl.c \
  ../../../Src/Render/GlBuffer.cpp \
  ../../../Src/Render/GlGeometry.cpp \
  ../../../Src/Render/GlProgram.cpp \
  ../../../Src/Render/GlSetup.cpp \
  ../../../Src/Render/GlTexture.cpp \
  ../../../Src/Render/PanelRenderer.cpp \
  ../../../Src/Render/ParticleSystem.cpp	\
  ../../../Src/Render/PointList.cpp \
  ../../../Src/Render/Ribbon.cpp \
  ../../../Src/Render/SurfaceRender.cpp \
  ../../../Src/Render/SurfaceTexture.cpp \
  ../../../Src/Render/TextureAtlas.cpp \
  ../../../Src/Render/TextureManager.cpp \
  ../../../Src/System.cpp \

LOCAL_STATIC_LIBRARIES += minizip stb android_native_app_glue

# start building based on everything since CLEAR_VARS
include $(BUILD_STATIC_LIBRARY)

$(call import-module,android/native_app_glue)
$(call import-module,3rdParty/minizip/build/android/jni)
$(call import-module,3rdParty/stb/build/android/jni)
