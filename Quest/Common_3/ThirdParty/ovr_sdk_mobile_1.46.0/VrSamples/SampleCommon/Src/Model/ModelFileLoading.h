/************************************************************************************

Filename    :   ModelFileLoading.h
Content     :   Model file loading.
Created     :   December 2013
Authors     :   John Carmack, J.M.P. van Waveren

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#pragma once
#include "ModelFile.h"

#include <math.h>
#include <vector>

#include "OVR_Math.h"

#include "unzip.h"

// Verbose log, redefine this as LOG() to get lots more info dumped
//#define LOGV ALOG
#define LOGV(...)

namespace OVRFW {

void CalculateTransformFromRTS(
    OVR::Matrix4f* localTransform,
    const OVR::Quatf rotation,
    const OVR::Vector3f translation,
    const OVR::Vector3f scale);

void LoadModelFileTexture(
    ModelFile& model,
    const char* textureName,
    const char* buffer,
    const int size,
    const MaterialParms& materialParms);

bool LoadModelFile_OvrScene(
    ModelFile* modelPtr,
    unzFile zfp,
    const char* fileName,
    const char* fileData,
    const int fileDataLength,
    const ModelGlPrograms& programs,
    const MaterialParms& materialParms,
    ModelGeo* outModelGeo = NULL);

bool LoadModelFile_glTF_OvrScene(
    ModelFile* modelFilePtr,
    unzFile zfp,
    const char* fileName,
    const char* fileData,
    const int fileDataLength,
    const ModelGlPrograms& programs,
    const MaterialParms& materialParms,
    ModelGeo* outModelGeo = NULL);

ModelFile* LoadModelFile_glB(
    const char* fileName,
    const char* fileData,
    const int fileDataLength,
    const ModelGlPrograms& programs,
    const MaterialParms& materialParms,
    ModelGeo* outModelGeo = NULL);

} // namespace OVRFW
