/************************************************************************************

Filename    :   ModelFile.cpp
Content     :   Model file loading common elements.
Created     :   December 2013
Authors     :   John Carmack, J.M.P. van Waveren

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "ModelFileLoading.h"

#include "PackageFiles.h"
#include "OVR_FileSys.h"
#include "OVR_MappedFile.h"

#include "OVR_Std.h"

#include "Misc/Log.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

//-----------------------------------------------------------------------------
//	ModelFile
//-----------------------------------------------------------------------------

ModelFile::ModelFile()
    : UsingSrgbTextures(false), animationStartTime(0.0f), animationEndTime(0.0f) {}

ModelFile::~ModelFile() {
    ALOG("Destroying ModelFileModel %s", FileName.c_str());

    for (int i = 0; i < static_cast<int>(Textures.size()); i++) {
        FreeTexture(Textures[i].texid);
    }

    for (int i = 0; i < static_cast<int>(Models.size()); i++) {
        for (int j = 0; j < static_cast<int>(Models[i].surfaces.size()); j++) {
            const_cast<GlGeometry*>(&(Models[i].surfaces[j].surfaceDef.geo))->Free();
        }
    }

    for (int i = 0; i < static_cast<int>(Buffers.size()); i++) {
        delete Buffers[i].bufferData;
    }
}

ovrSurfaceDef* ModelFile::FindNamedSurface(const char* name) const {
    for (int i = 0; i < static_cast<int>(Models.size()); i++) {
        for (int j = 0; j < static_cast<int>(Models[i].surfaces.size()); j++) {
            const ovrSurfaceDef& sd = Models[i].surfaces[j].surfaceDef;
            if (OVR::OVR_stricmp(sd.surfaceName.c_str(), name) == 0) {
                return const_cast<ovrSurfaceDef*>(&sd);
            }
        }
    }
    return nullptr;
}

const ModelTexture* ModelFile::FindNamedTexture(const char* name) const {
    for (int i = 0; i < static_cast<int>(Textures.size()); i++) {
        const ModelTexture& st = Textures[i];
        if (OVR::OVR_stricmp(st.name.c_str(), name) == 0) {
            return &st;
        }
    }
    return nullptr;
}

const ModelJoint* ModelFile::FindNamedJoint(const char* name) const {
    for (int i = 0; i < static_cast<int>(Nodes.size()); i++) {
        for (int j = 0; j < static_cast<int>(Nodes[i].JointsOvrScene.size()); j++) {
            const ModelJoint& joint = Nodes[i].JointsOvrScene[j];
            if (OVR::OVR_stricmp(joint.name.c_str(), name) == 0) {
                return &joint;
            }
        }
    }
    return nullptr;
}

const ModelTag* ModelFile::FindNamedTag(const char* name) const {
    for (int i = 0; i < static_cast<int>(Tags.size()); i++) {
        const ModelTag& tag = Tags[i];
        if (OVR::OVR_stricmp(tag.name.c_str(), name) == 0) {
            ALOG("Found named tag %s", name);
            return &tag;
        }
    }
    ALOG("Did not find named tag %s", name);
    return nullptr;
}

Bounds3f ModelFile::GetBounds() const {
    Bounds3f modelBounds;
    modelBounds.Clear();
    for (int i = 0; i < static_cast<int>(Models.size()); i++) {
        for (int j = 0; j < static_cast<int>(Models[i].surfaces.size()); j++) {
            const ovrSurfaceDef& sd = Models[i].surfaces[j].surfaceDef;
            modelBounds.AddPoint(sd.geo.localBounds.b[0]);
            modelBounds.AddPoint(sd.geo.localBounds.b[1]);
        }
    }
    return modelBounds;
}

void CalculateTransformFromRTS(
    Matrix4f* localTransform,
    const Quatf rotation,
    const Vector3f translation,
    const Vector3f scale) {
    Matrix4f::Multiply(localTransform, Matrix4f(rotation), Matrix4f::Scaling(scale));
    Matrix4f::Multiply(localTransform, Matrix4f::Translation(translation), *localTransform);
}

//-----------------------------------------------------------------------------
//	Model Loading
//-----------------------------------------------------------------------------

void LoadModelFileTexture(
    ModelFile& model,
    const char* textureName,
    const char* buffer,
    const int size,
    const MaterialParms& materialParms) {
    ModelTexture tex;
    tex.name = textureName;
    tex.name = tex.name.substr(0, tex.name.rfind("."));
    int width;
    int height;
    tex.texid = LoadTextureFromBuffer(
        textureName,
        (const uint8_t*)buffer,
        size,
        (materialParms.UseSrgbTextureFormats ? TextureFlags_t(TEXTUREFLAG_USE_SRGB)
                                             : TextureFlags_t()),
        width,
        height);

    // ALOG( ( tex.texid.target == GL_TEXTURE_CUBE_MAP ) ? "GL_TEXTURE_CUBE_MAP: %s" :
    // "GL_TEXTURE_2D: %s", textureName );

    // file name metadata for enabling clamp mode
    // Used for sky sides in Tuscany.
    if (strstr(textureName, "_c.")) {
        MakeTextureClamped(tex.texid);
    }

    model.Textures.push_back(tex);
}

static ModelFile* LoadZippedModelFile(
    unzFile zfp,
    const char* fileName,
    const char* fileData,
    const int fileDataLength,
    const ModelGlPrograms& programs,
    const MaterialParms& materialParms,
    ModelGeo* outModelGeo = nullptr) {
    // LOGCPUTIME( "LoadZippedModelFile" );

    ModelFile* modelFilePtr = new ModelFile;

    modelFilePtr->FileName = fileName;
    modelFilePtr->UsingSrgbTextures = materialParms.UseSrgbTextureFormats;

    bool loaded = false;

    if (!zfp) {
        ALOGW("Error: can't load %s", fileName);
    } else if (strstr(fileName, ".gltf.ovrscene") != nullptr) {
        loaded = LoadModelFile_glTF_OvrScene(
            modelFilePtr,
            zfp,
            fileName,
            fileData,
            fileDataLength,
            programs,
            materialParms,
            outModelGeo);
    } else {
        loaded = LoadModelFile_OvrScene(
            modelFilePtr,
            zfp,
            fileName,
            fileData,
            fileDataLength,
            programs,
            materialParms,
            outModelGeo);
    }

    if (!loaded) {
        ALOGW("Error: failed to load %s", fileName);
        delete modelFilePtr;
        modelFilePtr = nullptr;
    }

    if (modelFilePtr) {
        /// Bind the uniform data slots to the texture objects
        for (int i = 0; i < static_cast<int>(modelFilePtr->Models.size()); i++) {
            auto& m = modelFilePtr->Models[i];
            for (int j = 0; j < static_cast<int>(m.surfaces.size()); j++) {
                ovrGraphicsCommand& gc =
                    *const_cast<ovrGraphicsCommand*>(&m.surfaces[j].surfaceDef.graphicsCommand);
                gc.BindUniformTextures();
            }
        }
    }

    return modelFilePtr;
}

struct zlib_mmap_opaque {
    MappedFile file;
    MappedView view;
    const std::uint8_t* data;
    const std::uint8_t* ptr;
    int len;
    int left;
};

static voidpf ZCALLBACK mmap_fopen_file_func(voidpf opaque, const char*, int) {
    return opaque;
}

static uLong ZCALLBACK mmap_fread_file_func(voidpf opaque, voidpf, void* buf, uLong size) {
    zlib_mmap_opaque* state = (zlib_mmap_opaque*)opaque;

    if ((int)size <= 0 || state->left < (int)size) {
        return 0;
    }

    memcpy(buf, state->ptr, size);
    state->ptr += size;
    state->left -= size;

    return size;
}

static uLong ZCALLBACK mmap_fwrite_file_func(voidpf, voidpf, const void*, uLong) {
    return 0;
}

static long ZCALLBACK mmap_ftell_file_func(voidpf opaque, voidpf) {
    zlib_mmap_opaque* state = (zlib_mmap_opaque*)opaque;

    return state->len - state->left;
}

static long ZCALLBACK mmap_fseek_file_func(voidpf opaque, voidpf, uLong offset, int origin) {
    zlib_mmap_opaque* state = (zlib_mmap_opaque*)opaque;

    switch (origin) {
        case SEEK_SET:
            if ((int)offset < 0 || (int)offset > state->len) {
                return 0;
            }
            state->ptr = state->data + offset;
            state->left = state->len - offset;
            break;
        case SEEK_CUR:
            if ((int)offset < 0 || (int)offset > state->left) {
                return 0;
            }
            state->ptr += offset;
            state->left -= offset;
            break;
        case SEEK_END:
            state->ptr = state->data + state->len;
            state->left = 0;
            break;
    }

    return 0;
}

static int ZCALLBACK mmap_fclose_file_func(voidpf, voidpf) {
    return 0;
}

static int ZCALLBACK mmap_ferror_file_func(voidpf, voidpf) {
    return 0;
}

static void mem_set_opaque(zlib_mmap_opaque& opaque, const unsigned char* data, int len) {
    opaque.data = data;
    opaque.len = len;
    opaque.ptr = data;
    opaque.left = len;
}

static bool mmap_open_opaque(const char* fileName, zlib_mmap_opaque& opaque) {
    // If unable to open the ZIP file,
    if (!opaque.file.OpenRead(fileName, true, true)) {
        ALOGW("Couldn't open %s", fileName);
        return false;
    }

    int len = (int)opaque.file.GetLength();
    if (len <= 0) {
        ALOGW("len = %i", len);
        return false;
    }
    if (!opaque.view.Open(&opaque.file)) {
        ALOGW("View open failed");
        return false;
    }
    if (!opaque.view.MapView(0, len)) {
        ALOGW("MapView failed");
        return false;
    }

    opaque.data = opaque.view.GetFront();
    opaque.len = len;
    opaque.ptr = opaque.data;
    opaque.left = len;

    return true;
}

static unzFile open_opaque(zlib_mmap_opaque& zlib_opaque, const char* fileName) {
    zlib_filefunc_def zlib_file_funcs = {
        mmap_fopen_file_func,
        mmap_fread_file_func,
        mmap_fwrite_file_func,
        mmap_ftell_file_func,
        mmap_fseek_file_func,
        mmap_fclose_file_func,
        mmap_ferror_file_func,
        &zlib_opaque};

    return unzOpen2(fileName, &zlib_file_funcs);
}

ModelFile* LoadModelFileFromMemory(
    const char* fileName,
    const void* buffer,
    int bufferLength,
    const ModelGlPrograms& programs,
    const MaterialParms& materialParms,
    ModelGeo* outModelGeo) {
    // Open the .ModelFile file as a zip.
    ALOG("LoadModelFileFromMemory %s %i", fileName, bufferLength);

    // Determine wether it's a glb binary file, or if it is a zipped up ovrscene.
    if (strstr(fileName, ".glb") != nullptr) {
        return LoadModelFile_glB(
            fileName, (char*)buffer, bufferLength, programs, materialParms, outModelGeo);
    }

    zlib_mmap_opaque zlib_opaque;

    mem_set_opaque(zlib_opaque, (const unsigned char*)buffer, bufferLength);

    unzFile zfp = open_opaque(zlib_opaque, fileName);
    if (!zfp) {
        ALOGW("could not open file %s", fileName);
        return nullptr;
    }

    ALOG("LoadModelFileFromMemory zfp = %p", zfp);

    return LoadZippedModelFile(
        zfp, fileName, (char*)buffer, bufferLength, programs, materialParms, outModelGeo);
}

ModelFile* LoadModelFile(
    const char* fileName,
    const ModelGlPrograms& programs,
    const MaterialParms& materialParms) {
    ALOG("LoadModelFile %s", fileName);

    zlib_mmap_opaque zlib_opaque;

    // Map and open the zip file
    if (!mmap_open_opaque(fileName, zlib_opaque)) {
        ALOGW("could not map file %s", fileName);
        return nullptr;
    }

    // Determine wether it's a glb binary file, or if it is a zipped up ovrscene.
    if (strstr(fileName, ".glb") != nullptr) {
        return LoadModelFile_glB(
            fileName, (char*)zlib_opaque.data, zlib_opaque.len, programs, materialParms);
    }

    unzFile zfp = open_opaque(zlib_opaque, fileName);
    if (!zfp) {
        ALOGW("could not open file %s", fileName);
        return nullptr;
    }

    return LoadZippedModelFile(
        zfp, fileName, (char*)zlib_opaque.data, zlib_opaque.len, programs, materialParms);
}

ModelFile* LoadModelFileFromOtherApplicationPackage(
    void* zipFile,
    const char* nameInZip,
    const ModelGlPrograms& programs,
    const MaterialParms& materialParms) {
    void* buffer;
    int bufferLength;

    ovr_ReadFileFromOtherApplicationPackage(zipFile, nameInZip, bufferLength, buffer);
    if (buffer == nullptr) {
        ALOGW("Failed to load model file '%s' from apk", nameInZip);
        return nullptr;
    }

    ModelFile* scene =
        LoadModelFileFromMemory(nameInZip, buffer, bufferLength, programs, materialParms);

    free(buffer);
    return scene;
}

ModelFile* LoadModelFileFromApplicationPackage(
    const char* nameInZip,
    const ModelGlPrograms& programs,
    const MaterialParms& materialParms) {
    return LoadModelFileFromOtherApplicationPackage(
        ovr_GetApplicationPackageFile(), nameInZip, programs, materialParms);
}

ModelFile* LoadModelFile(
    ovrFileSys& fileSys,
    const char* uri,
    const ModelGlPrograms& programs,
    const MaterialParms& materialParms) {
    std::vector<uint8_t> buffer;
    if (!fileSys.ReadFile(uri, buffer)) {
        ALOGW("Failed to load model uri '%s'", uri);
        return nullptr;
    }
    ModelFile* scene = LoadModelFileFromMemory(
        uri, buffer.data(), static_cast<int>(buffer.size()), programs, materialParms);
    return scene;
}

uint8_t* ModelAccessor::BufferData() const {
    if (bufferView == nullptr || bufferView->buffer == nullptr ||
        bufferView->buffer->bufferData == nullptr) {
        return nullptr;
    }
    return bufferView->buffer->bufferData + bufferView->byteOffset + byteOffset;
}

void ModelNode::SetLocalTransform(const Matrix4f matrix) {
    localTransform = matrix;
}

void ModelNode::RecalculateGlobalTransform(ModelFile& modelFile) {
    if (parentIndex < 0) {
        globalTransform = localTransform;
    } else {
        globalTransform = modelFile.Nodes[parentIndex].GetGlobalTransform() * localTransform;
    }

    for (int i = 0; i < static_cast<int>(children.size()); i++) {
        modelFile.Nodes[children[i]].RecalculateGlobalTransform(modelFile);
    }
}

void ModelAnimationTimeLine::Initialize(const ModelAccessor* _accessor) {
    accessor = _accessor;
    sampleCount = accessor->count;
    sampleTimes = (float*)(accessor->BufferData());
    startTime = sampleTimes[0];
    endTime = sampleTimes[sampleCount - 1];
    float duration = endTime - startTime;
    const float step = duration / sampleCount;
    rcpStep = 1.0f / step;
    for (int keyFrameIndex = 0; keyFrameIndex < sampleCount; keyFrameIndex++) {
        const float delta =
            sampleTimes[keyFrameIndex] - (((float)keyFrameIndex) * step + startTime);
        // Check if the time is more than 0.1 milliseconds from a fixed-rate time-line.
        if (fabs(delta) > 1e-4f) {
            rcpStep = 0.0f;
            break;
        }
    }
}

void ModelAnimationTimeLineState::CalculateFrameAndFraction(float timeInSeconds) {
    if (timeInSeconds <= timeline->startTime) {
        frame = 0;
        fraction = 0.0f;
    } else if (timeInSeconds >= timeline->endTime) {
        frame = timeline->sampleCount - 2;
        fraction = 1.0f;
    } else {
        if (timeline->rcpStep != 0.0f) {
            // Use direct lookup if this is a fixed rate animation.
            frame = (int)((timeInSeconds - timeline->startTime) * timeline->rcpStep);
        } else {
            // Use a binary search to find the key frame.
            frame = 0;
            // Use a binary search to find the key frame.
            for (int sampleCount = timeline->sampleCount; sampleCount > 1; sampleCount >>= 1) {
                const int mid = sampleCount >> 1;
                if (timeInSeconds >= timeline->sampleTimes[frame + mid]) {
                    frame += mid;
                    sampleCount = (sampleCount - mid) * 2;
                }
            }
        }

        fraction = (timeInSeconds - timeline->sampleTimes[frame]) /
            (timeline->sampleTimes[frame + 1] - timeline->sampleTimes[frame]);
    }
}

void ModelState::CalculateAnimationFrameAndFraction(
    const ModelAnimationTimeType type,
    float timeInSeconds) {
    // float prevTime = timeInSeconds;
    switch (type) {
        case MODEL_ANIMATION_TIME_TYPE_ONCE_FORWARD: {
            if (timeInSeconds > mf->animationEndTime) {
                timeInSeconds = mf->animationEndTime;
            }
        } break;
        case MODEL_ANIMATION_TIME_TYPE_LOOP_FORWARD: {
            timeInSeconds = fmodf(timeInSeconds, mf->animationEndTime);
        } break;
        case MODEL_ANIMATION_TIME_TYPE_LOOP_FORWARD_AND_BACK: {
            const float tempDur = mf->animationEndTime * 2.0f;
            timeInSeconds = fmodf(timeInSeconds, tempDur);

            if (timeInSeconds > mf->animationEndTime) {
                timeInSeconds = timeInSeconds - mf->animationEndTime;
                timeInSeconds = mf->animationEndTime - timeInSeconds;
            }
        } break;
    }

    for (int i = 0; i < static_cast<int>(animationTimelineStates.size()); i++) {
        animationTimelineStates[i].CalculateFrameAndFraction(timeInSeconds);
    }
}

void ModelNodeState::GenerateStateFromNode(const ModelNode* _node, ModelState* _modelState) {
    node = _node;
    state = _modelState;
    rotation = node->rotation;
    translation = node->translation;
    scale = node->scale;

    // These values should be calculated already.
    localTransform = node->GetLocalTransform();
    globalTransform = node->GetGlobalTransform();
}

void ModelNodeState::CalculateLocalTransform() {
    CalculateTransformFromRTS(&localTransform, rotation, translation, scale);
}

void ModelNodeState::SetLocalTransform(const Matrix4f matrix) {
    localTransform = matrix;
}

void ModelNodeState::RecalculateMatrix() {
    if (node->parentIndex < 0) {
        globalTransform = state->GetMatrix() * localTransform;
    } else {
        globalTransform = state->nodeStates[node->parentIndex].globalTransform * localTransform;
    }

    for (int i = 0; i < static_cast<int>(node->children.size()); i++) {
        state->nodeStates[node->children[i]].RecalculateMatrix();
    }
}

void ModelNodeState::AddNodesToEmitList(std::vector<ModelNodeState*>& emitList) {
    emitList.push_back(this);
    for (int i = 0; i < static_cast<int>(node->children.size()); i++) {
        state->nodeStates[node->children[i]].AddNodesToEmitList(emitList);
    }
}

void ModelSubSceneState::GenerateStateFromSubScene(const ModelSubScene* _subScene) {
    subScene = _subScene;
    visible = subScene->visible;

    nodeStates.resize(subScene->nodes.size());
    for (int i = 0; i < static_cast<int>(subScene->nodes.size()); i++) {
        nodeStates[i] = subScene->nodes[i];
    }
}

void ModelState::GenerateStateFromModelFile(const ModelFile* _mf) {
    subSceneStates.clear();
    modelMatrix = Matrix4f::Identity();

    mf = _mf;
    DontRenderForClientUid = 0;

    nodeStates.resize(mf->Nodes.size());
    for (int i = 0; i < static_cast<int>(mf->Nodes.size()); i++) {
        nodeStates[i].GenerateStateFromNode(&mf->Nodes[i], this);
    }

    animationTimelineStates.resize(mf->AnimationTimeLines.size());
    for (int i = 0; i < static_cast<int>(mf->AnimationTimeLines.size()); i++) {
        animationTimelineStates[i].timeline = &mf->AnimationTimeLines[i];
    }

    subSceneStates.resize(mf->SubScenes.size());
    for (int i = 0; i < static_cast<int>(mf->SubScenes.size()); i++) {
        subSceneStates[i].GenerateStateFromSubScene(&mf->SubScenes[i]);
    }
}

void ModelState::SetMatrix(const Matrix4f matrix) {
    modelMatrix = matrix;
    for (int i = 0; i < static_cast<int>(subSceneStates.size()); i++) {
        for (int j = 0; j < static_cast<int>(subSceneStates[i].nodeStates.size()); j++) {
            nodeStates[subSceneStates[i].nodeStates[j]].RecalculateMatrix();
        }
    }
}

} // namespace OVRFW
