/************************************************************************************

Filename    :   SurfaceTexture.cpp
Content     :   Interface to Android SurfaceTexture objects
Created     :   September 17, 2013
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "SurfaceTexture.h"

#include <stdlib.h>

#include "Misc/Log.h"
#include "Egl.h"
#include "GlTexture.h"

namespace OVRFW {

SurfaceTexture::SurfaceTexture(JNIEnv* jni_)
    : textureId(0),
      javaObject(NULL),
      jni(NULL),
      nanoTimeStamp(0),
      updateTexImageMethodId(NULL),
      getTimestampMethodId(NULL),
      setDefaultBufferSizeMethodId(NULL) {
    jni = jni_;

    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, GetTextureId());
    glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);

    static const char* className = "android/graphics/SurfaceTexture";
    JavaClass surfaceTextureClass(jni, jni->FindClass(className));
    if (0 == surfaceTextureClass.GetJClass()) {
        ALOGE_FAIL("FindClass( %s ) failed", className);
    }

    // find the constructor that takes an int
    const jmethodID constructor =
        jni->GetMethodID(surfaceTextureClass.GetJClass(), "<init>", "(I)V");
    if (constructor == 0) {
        ALOGE_FAIL("GetMethodID( <init> ) failed");
    }
    updateTexImageMethodId =
        jni->GetMethodID(surfaceTextureClass.GetJClass(), "updateTexImage", "()V");
    if (updateTexImageMethodId == 0) {
        ALOGE_FAIL("couldn't get updateTexImageMethodId");
    }
    getTimestampMethodId = jni->GetMethodID(surfaceTextureClass.GetJClass(), "getTimestamp", "()J");
    if (getTimestampMethodId == 0) {
        ALOGE_FAIL("couldn't get getTimestampMethodId");
    }
    setDefaultBufferSizeMethodId =
        jni->GetMethodID(surfaceTextureClass.GetJClass(), "setDefaultBufferSize", "(II)V");
    if (setDefaultBufferSizeMethodId == 0) {
        ALOGE_FAIL("couldn't get setDefaultBufferSize");
    }

    JavaObject obj(
        jni, jni->NewObject(surfaceTextureClass.GetJClass(), constructor, GetTextureId()));
    if (obj.GetJObject() == 0) {
        ALOGE_FAIL("NewObject() failed");
    }

    /// Keep globar ref around
    javaObject = jni->NewGlobalRef(obj.GetJObject());
    if (javaObject == 0) {
        ALOGE_FAIL("NewGlobalRef() failed");
    }
}

SurfaceTexture::~SurfaceTexture() {
    if (textureId != 0) {
        glDeleteTextures(1, &textureId);
        textureId = 0;
    }
    if (javaObject) {
        jni->DeleteGlobalRef(javaObject);
        javaObject = 0;
    }
}

void SurfaceTexture::SetDefaultBufferSize(const int width, const int height) {
    jni->CallVoidMethod(javaObject, setDefaultBufferSizeMethodId, width, height);
}

void SurfaceTexture::Update() {
    // latch the latest movie frame to the texture
    if (!javaObject) {
        return;
    }

    jni->CallVoidMethod(javaObject, updateTexImageMethodId);
    nanoTimeStamp = jni->CallLongMethod(javaObject, getTimestampMethodId);
}

unsigned int SurfaceTexture::GetTextureId() {
    return textureId;
}

jobject SurfaceTexture::GetJavaObject() {
    return javaObject;
}

long long SurfaceTexture::GetNanoTimeStamp() {
    return nanoTimeStamp;
}

} // namespace OVRFW
