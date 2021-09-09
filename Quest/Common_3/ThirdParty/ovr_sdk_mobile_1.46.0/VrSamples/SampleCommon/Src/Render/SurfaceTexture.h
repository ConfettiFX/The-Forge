/************************************************************************************

Filename    :   SurfaceTexture.h
Content     :   Interface to Android SurfaceTexture objects
Created     :   September 17, 2013
Authors		:	John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/
#pragma once

#include "JniUtils.h"

namespace OVRFW {

// SurfaceTextures are used to get movie frames, Camera
// previews, and Android canvas views.
//
// Note that these never have mipmaps, so you will often
// want to render them to another texture and generate mipmaps
// to avoid aliasing when drawing, unless you know it will
// always be magnified.
//
// Note that we do not get and use the TransformMatrix
// from java.  Presumably this was only necessary before
// non-power-of-two textures became ubiquitous.
class SurfaceTexture {
   public:
    SurfaceTexture(JNIEnv* jni_);
    ~SurfaceTexture();

    // For some java-side uses, you can set the size
    // of the buffer before it is used to control how
    // large it is.  Video decompression and camera preview
    // always override the size automatically.
    void SetDefaultBufferSize(const int width, const int height);

    // This can only be called with an active GL context.
    // As a side effect, the textureId will be bound to the
    // GL_TEXTURE_EXTERNAL_OES target of the currently active
    // texture unit.
    void Update();

    unsigned int GetTextureId();
    jobject GetJavaObject();
    long long GetNanoTimeStamp();

   private:
    unsigned int textureId;
    jobject javaObject;
    JNIEnv* jni;

    // Updated when Update() is called, can be used to
    // check if a new frame is available and ready
    // to be processed / mipmapped by other code.
    long long nanoTimeStamp;

    jmethodID updateTexImageMethodId;
    jmethodID getTimestampMethodId;
    jmethodID setDefaultBufferSizeMethodId;
};

} // namespace OVRFW
