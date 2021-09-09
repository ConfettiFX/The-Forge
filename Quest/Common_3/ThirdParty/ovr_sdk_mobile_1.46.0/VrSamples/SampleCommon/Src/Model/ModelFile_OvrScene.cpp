/************************************************************************************

Filename    :   ModelFile_OvrScene.cpp
Content     :   Model file loading ovrscene elements.
Created     :   December 2013
Authors     :   John Carmack, J.M.P. van Waveren

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "ModelFileLoading.h"

#include "Render/GlGeometry.h"

#include "OVR_Std.h"
#include "OVR_JSON.h"
#include "StringUtils.h"

#include "Misc/Log.h"
#include "OVR_BinaryFile2.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

template <typename _type_>
void ReadModelArray(
    std::vector<_type_>& out,
    const char* string,
    const BinaryReader& bin,
    const int numElements) {
    if (string != nullptr && string[0] != '\0' && numElements > 0) {
        if (!bin.ReadArray(out, numElements)) {
            OVR::StringUtils::StringTo(out, string);
        }
    }
}

bool LoadModelFile_OvrScene_Json(
    ModelFile& modelFile,
    const char* modelsJson,
    const int modelsJsonLength,
    const char* modelsBin,
    const int modelsBinLength,
    const ModelGlPrograms& programs,
    const MaterialParms& materialParms,
    ModelGeo* outModelGeo) {
    ALOG("parsing %s", modelFile.FileName.c_str());
    OVR_UNUSED(modelsJsonLength);

    const BinaryReader bin((const std::uint8_t*)modelsBin, modelsBinLength);

    if (modelsBin != nullptr && bin.ReadUInt32() != 0x6272766F) {
        ALOGW("LoadModelFile_OvrScene_Json: bad binary file for %s", modelFile.FileName.c_str());
        return false;
    }

    const char* error = nullptr;
    auto json = OVR::JSON::Parse(modelsJson, &error);
    if (json == nullptr) {
        ALOGW(
            "LoadModelFile_OvrScene_Json: Error loading %s : %s",
            modelFile.FileName.c_str(),
            error);
        return false;
    }

    if (modelFile.SubScenes.size() > 0 || modelFile.Nodes.size() > 0 ||
        modelFile.Models.size() > 0) {
        ALOGW(
            "LoadModelFile_OvrScene_Json: model already has data, replacing with %s",
            modelFile.FileName.c_str());
        modelFile.SubScenes.clear();
        modelFile.Nodes.clear();
        modelFile.Models.clear();
    }

    const OVR::JsonReader models(json);
    if (models.IsObject()) {
        // For OvrScene we will generate just one subScene, node, and model to place the surfaces
        // on.
        const size_t modelIndex = static_cast<size_t>(modelFile.Models.size());
        modelFile.Models.emplace_back(Model());
        modelFile.Models[modelIndex].name = "DefaultModelName";
        const size_t nodeIndex = static_cast<size_t>(modelFile.Nodes.size());
        modelFile.Nodes.emplace_back(ModelNode());
        modelFile.Nodes[nodeIndex].name = "DefaultNodeName";
        modelFile.Nodes[nodeIndex].model = &modelFile.Models[modelIndex];
        const size_t sceneIndex = static_cast<size_t>(modelFile.SubScenes.size());
        modelFile.SubScenes.emplace_back(ModelSubScene());
        modelFile.SubScenes[sceneIndex].name = "DefaultSceneName";
        modelFile.SubScenes[sceneIndex].visible = true;
        modelFile.SubScenes[sceneIndex].nodes.push_back((int)nodeIndex);

        //
        // Render Model
        //

        const OVR::JsonReader render_model(models.GetChildByName("render_model"));
        if (render_model.IsObject()) {
            LOGV("loading render model..");

            //
            // Render Model Textures
            //

            enum TextureOcclusion {
                TEXTURE_OCCLUSION_OPAQUE,
                TEXTURE_OCCLUSION_PERFORATED,
                TEXTURE_OCCLUSION_TRANSPARENT
            };

            std::vector<GlTexture> glTextures;

            const OVR::JsonReader texture_array(render_model.GetChildByName("textures"));
            if (texture_array.IsArray()) {
                while (!texture_array.IsEndOfArray()) {
                    const OVR::JsonReader texture(texture_array.GetNextArrayElement());
                    if (texture.IsObject()) {
                        const std::string name = texture.GetChildStringByName("name");

                        // Try to match the texture names with the already loaded texture
                        // and create a default texture if the texture file is missing.
                        int i = 0;
                        for (; i < static_cast<int>(modelFile.Textures.size()); i++) {
                            if (OVR::OVR_stricmp(
                                    modelFile.Textures[i].name.c_str(), name.c_str()) == 0) {
                                break;
                            }
                        }
                        if (i == static_cast<int>(modelFile.Textures.size())) {
                            ALOG("texture %s defaulted", name.c_str());
                            // Create a default texture.
                            LoadModelFileTexture(
                                modelFile, name.c_str(), nullptr, 0, materialParms);
                        }
                        glTextures.push_back(modelFile.Textures[i].texid);

                        const std::string usage = texture.GetChildStringByName("usage");
                        if (usage == "diffuse") {
                            if (materialParms.EnableDiffuseAniso == true) {
                                MakeTextureAniso(modelFile.Textures[i].texid, 2.0f);
                            }
                        } else if (usage == "emissive") {
                            if (materialParms.EnableEmissiveLodClamp == true) {
                                // LOD clamp lightmap textures to avoid light bleeding
                                MakeTextureLodClamped(modelFile.Textures[i].texid, 1);
                            }
                        }
                        /*
                        const std::string occlusion = texture.GetChildStringByName( "occlusion" );

                        TextureOcclusion textureOcclusion = TEXTURE_OCCLUSION_OPAQUE;
                        if ( occlusion == "opaque" )			{ textureOcclusion =
                        TEXTURE_OCCLUSION_OPAQUE; } else if ( occlusion == "perforated" )	{
                        textureOcclusion = TEXTURE_OCCLUSION_PERFORATED; } else if ( occlusion ==
                        "transparent" )	{ textureOcclusion = TEXTURE_OCCLUSION_TRANSPARENT; }
                        */
                    }
                }
            }

            //
            // Render Model Joints
            //

            const OVR::JsonReader joint_array(render_model.GetChildByName("joints"));
            if (joint_array.IsArray()) {
                while (!joint_array.IsEndOfArray()) {
                    const OVR::JsonReader joint(joint_array.GetNextArrayElement());
                    if (joint.IsObject()) {
                        const size_t index =
                            static_cast<size_t>(modelFile.Nodes[nodeIndex].JointsOvrScene.size());
                        modelFile.Nodes[nodeIndex].JointsOvrScene.emplace_back(ModelJoint());
                        modelFile.Nodes[nodeIndex].JointsOvrScene[index].index =
                            static_cast<int>(index);
                        modelFile.Nodes[nodeIndex].JointsOvrScene[index].name =
                            joint.GetChildStringByName("name");
                        OVR::StringUtils::StringTo(
                            modelFile.Nodes[nodeIndex].JointsOvrScene[index].transform,
                            joint.GetChildStringByName("transform").c_str());
                        modelFile.Nodes[nodeIndex].JointsOvrScene[index].animation =
                            MODEL_JOINT_ANIMATION_NONE;
                        const std::string animation = joint.GetChildStringByName("animation");
                        if (animation == "none") {
                            modelFile.Nodes[nodeIndex].JointsOvrScene[index].animation =
                                MODEL_JOINT_ANIMATION_NONE;
                        } else if (animation == "rotate") {
                            modelFile.Nodes[nodeIndex].JointsOvrScene[index].animation =
                                MODEL_JOINT_ANIMATION_ROTATE;
                        } else if (animation == "sway") {
                            modelFile.Nodes[nodeIndex].JointsOvrScene[index].animation =
                                MODEL_JOINT_ANIMATION_SWAY;
                        } else if (animation == "bob") {
                            modelFile.Nodes[nodeIndex].JointsOvrScene[index].animation =
                                MODEL_JOINT_ANIMATION_BOB;
                        }
                        modelFile.Nodes[nodeIndex].JointsOvrScene[index].parameters.x =
                            joint.GetChildFloatByName("parmX");
                        modelFile.Nodes[nodeIndex].JointsOvrScene[index].parameters.y =
                            joint.GetChildFloatByName("parmY");
                        modelFile.Nodes[nodeIndex].JointsOvrScene[index].parameters.z =
                            joint.GetChildFloatByName("parmZ");
                        modelFile.Nodes[nodeIndex].JointsOvrScene[index].timeOffset =
                            joint.GetChildFloatByName("timeOffset");
                        modelFile.Nodes[nodeIndex].JointsOvrScene[index].timeScale =
                            joint.GetChildFloatByName("timeScale");
                    }
                }
            }

            //
            // Render Model Tags
            //

            const OVR::JsonReader tag_array(render_model.GetChildByName("tags"));
            if (tag_array.IsArray()) {
                modelFile.Tags.clear();

                while (!tag_array.IsEndOfArray()) {
                    const OVR::JsonReader tag(tag_array.GetNextArrayElement());
                    if (tag.IsObject()) {
                        const size_t index = static_cast<size_t>(modelFile.Tags.size());
                        modelFile.Tags.emplace_back(ModelTag());
                        modelFile.Tags[index].name = tag.GetChildStringByName("name");
                        OVR::StringUtils::StringTo(
                            modelFile.Tags[index].matrix,
                            tag.GetChildStringByName("matrix").c_str());
                        OVR::StringUtils::StringTo(
                            modelFile.Tags[index].jointIndices,
                            tag.GetChildStringByName("jointIndices").c_str());
                        OVR::StringUtils::StringTo(
                            modelFile.Tags[index].jointWeights,
                            tag.GetChildStringByName("jointWeights").c_str());
                    }
                }
            }

            //
            // Render Model Surfaces
            //

            const OVR::JsonReader surface_array(render_model.GetChildByName("surfaces"));
            if (surface_array.IsArray()) {
                while (!surface_array.IsEndOfArray()) {
                    const OVR::JsonReader surface(surface_array.GetNextArrayElement());
                    if (surface.IsObject()) {
                        ModelSurface modelSurface;

                        //
                        // Source Meshes
                        //

                        const OVR::JsonReader source(surface.GetChildByName("source"));
                        if (source.IsArray()) {
                            while (!source.IsEndOfArray()) {
                                if (modelSurface.surfaceDef.surfaceName.length()) {
                                    modelSurface.surfaceDef.surfaceName += ";";
                                }
                                modelSurface.surfaceDef.surfaceName += source.GetNextArrayString();
                            }
                        }

                        LOGV("surface %s", modelSurface.surfaceDef.surfaceName.c_str());

                        //
                        // Surface Material
                        //

                        enum {
                            MATERIAL_TYPE_OPAQUE,
                            MATERIAL_TYPE_PERFORATED,
                            MATERIAL_TYPE_TRANSPARENT,
                            MATERIAL_TYPE_ADDITIVE
                        } materialType = MATERIAL_TYPE_OPAQUE;

                        int diffuseTextureIndex = -1;
                        int normalTextureIndex = -1;
                        int specularTextureIndex = -1;
                        int emissiveTextureIndex = -1;
                        int reflectionTextureIndex = -1;

                        const OVR::JsonReader material(surface.GetChildByName("material"));
                        if (material.IsObject()) {
                            const std::string type = material.GetChildStringByName("type");

                            if (type == "opaque") {
                                materialType = MATERIAL_TYPE_OPAQUE;
                            } else if (type == "perforated") {
                                materialType = MATERIAL_TYPE_PERFORATED;
                            } else if (type == "transparent") {
                                materialType = MATERIAL_TYPE_TRANSPARENT;
                            } else if (type == "additive") {
                                materialType = MATERIAL_TYPE_ADDITIVE;
                            }

                            diffuseTextureIndex = material.GetChildInt32ByName("diffuse", -1);
                            normalTextureIndex = material.GetChildInt32ByName("normal", -1);
                            specularTextureIndex = material.GetChildInt32ByName("specular", -1);
                            emissiveTextureIndex = material.GetChildInt32ByName("emissive", -1);
                            reflectionTextureIndex = material.GetChildInt32ByName("reflection", -1);
                        }

                        //
                        // Surface Bounds
                        //

                        OVR::StringUtils::StringTo(
                            modelSurface.surfaceDef.geo.localBounds,
                            surface.GetChildStringByName("bounds").c_str());

                        TriangleIndex indexOffset = 0;
                        if (outModelGeo != nullptr) {
                            indexOffset =
                                static_cast<TriangleIndex>((*outModelGeo).positions.size());
                        }
                        //
                        // Vertices
                        //

                        VertexAttribs attribs;

                        const OVR::JsonReader vertices(surface.GetChildByName("vertices"));
                        if (vertices.IsObject()) {
                            const int vertexCount = std::min<int>(
                                vertices.GetChildInt32ByName("vertexCount"),
                                GlGeometry::GetMaxGeometryVertices());
                            // ALOG( "%5d vertices", vertexCount );

                            ReadModelArray(
                                attribs.position,
                                vertices.GetChildStringByName("position").c_str(),
                                bin,
                                vertexCount);
                            ReadModelArray(
                                attribs.normal,
                                vertices.GetChildStringByName("normal").c_str(),
                                bin,
                                vertexCount);
                            ReadModelArray(
                                attribs.tangent,
                                vertices.GetChildStringByName("tangent").c_str(),
                                bin,
                                vertexCount);
                            ReadModelArray(
                                attribs.binormal,
                                vertices.GetChildStringByName("binormal").c_str(),
                                bin,
                                vertexCount);
                            ReadModelArray(
                                attribs.color,
                                vertices.GetChildStringByName("color").c_str(),
                                bin,
                                vertexCount);
                            ReadModelArray(
                                attribs.uv0,
                                vertices.GetChildStringByName("uv0").c_str(),
                                bin,
                                vertexCount);
                            ReadModelArray(
                                attribs.uv1,
                                vertices.GetChildStringByName("uv1").c_str(),
                                bin,
                                vertexCount);
                            ReadModelArray(
                                attribs.jointIndices,
                                vertices.GetChildStringByName("jointIndices").c_str(),
                                bin,
                                vertexCount);
                            ReadModelArray(
                                attribs.jointWeights,
                                vertices.GetChildStringByName("jointWeights").c_str(),
                                bin,
                                vertexCount);

                            if (outModelGeo != nullptr) {
                                for (int i = 0; i < static_cast<int>(attribs.position.size());
                                     ++i) {
                                    (*outModelGeo).positions.push_back(attribs.position[i]);
                                }
                            }
                        }

                        //
                        // Triangles
                        //

                        std::vector<TriangleIndex> indices;

                        const OVR::JsonReader triangles(surface.GetChildByName("triangles"));
                        if (triangles.IsObject()) {
                            const int indexCount = std::min<int>(
                                triangles.GetChildInt32ByName("indexCount"),
                                GlGeometry::GetMaxGeometryIndices());
                            // ALOG( "%5d indices", indexCount );

                            ReadModelArray(
                                indices,
                                triangles.GetChildStringByName("indices").c_str(),
                                bin,
                                indexCount);
                        }

                        if (outModelGeo != nullptr) {
                            for (int i = 0; i < static_cast<int>(indices.size()); ++i) {
                                (*outModelGeo).indices.push_back(indices[i] + indexOffset);
                            }
                        }

                        //
                        // Setup geometry, textures and render programs now that the vertex
                        // attributes are known.
                        //

                        modelSurface.surfaceDef.geo.Create(attribs, indices);

                        const char* materialTypeString = "opaque";
                        OVR_UNUSED(
                            materialTypeString); // we'll get warnings if the LOGV's compile out

                        // set up additional material flags for the surface
                        if (materialType == MATERIAL_TYPE_PERFORATED) {
                            // Just blend because alpha testing is rather expensive.
                            modelSurface.surfaceDef.graphicsCommand.GpuState.blendEnable =
                                ovrGpuState::BLEND_ENABLE;
                            modelSurface.surfaceDef.graphicsCommand.GpuState.depthMaskEnable =
                                false;
                            modelSurface.surfaceDef.graphicsCommand.GpuState.blendSrc =
                                GL_SRC_ALPHA;
                            modelSurface.surfaceDef.graphicsCommand.GpuState.blendDst =
                                GL_ONE_MINUS_SRC_ALPHA;
                            materialTypeString = "perforated";
                        } else if (
                            materialType == MATERIAL_TYPE_TRANSPARENT ||
                            materialParms.Transparent) {
                            modelSurface.surfaceDef.graphicsCommand.GpuState.blendEnable =
                                ovrGpuState::BLEND_ENABLE;
                            modelSurface.surfaceDef.graphicsCommand.GpuState.depthMaskEnable =
                                false;
                            modelSurface.surfaceDef.graphicsCommand.GpuState.blendSrc =
                                GL_SRC_ALPHA;
                            modelSurface.surfaceDef.graphicsCommand.GpuState.blendDst =
                                GL_ONE_MINUS_SRC_ALPHA;
                            materialTypeString = "transparent";
                        } else if (materialType == MATERIAL_TYPE_ADDITIVE) {
                            modelSurface.surfaceDef.graphicsCommand.GpuState.blendEnable =
                                ovrGpuState::BLEND_ENABLE;
                            modelSurface.surfaceDef.graphicsCommand.GpuState.depthMaskEnable =
                                false;
                            modelSurface.surfaceDef.graphicsCommand.GpuState.blendSrc = GL_ONE;
                            modelSurface.surfaceDef.graphicsCommand.GpuState.blendDst = GL_ONE;
                            materialTypeString = "additive";
                        }

                        const bool skinned =
                            (attribs.jointIndices.size() == attribs.position.size() &&
                             attribs.jointWeights.size() == attribs.position.size());

                        if (diffuseTextureIndex >= 0 &&
                            diffuseTextureIndex < static_cast<int>(glTextures.size())) {
                            modelSurface.surfaceDef.graphicsCommand.Textures[0] =
                                glTextures[diffuseTextureIndex];

                            if (emissiveTextureIndex >= 0 &&
                                emissiveTextureIndex < static_cast<int>(glTextures.size())) {
                                modelSurface.surfaceDef.graphicsCommand.Textures[1] =
                                    glTextures[emissiveTextureIndex];

                                if (normalTextureIndex >= 0 &&
                                    normalTextureIndex < static_cast<int>(glTextures.size()) &&
                                    specularTextureIndex >= 0 &&
                                    specularTextureIndex < static_cast<int>(glTextures.size()) &&
                                    reflectionTextureIndex >= 0 &&
                                    reflectionTextureIndex < static_cast<int>(glTextures.size())) {
                                    // reflection mapped material;
                                    modelSurface.surfaceDef.graphicsCommand.Textures[2] =
                                        glTextures[normalTextureIndex];
                                    modelSurface.surfaceDef.graphicsCommand.Textures[3] =
                                        glTextures[specularTextureIndex];
                                    modelSurface.surfaceDef.graphicsCommand.Textures[4] =
                                        glTextures[reflectionTextureIndex];

                                    if (skinned) {
                                        if (programs.ProgSkinnedReflectionMapped == nullptr) {
                                            ALOGE_FAIL("No ProgSkinnedReflectionMapped set");
                                        }
                                        modelSurface.surfaceDef.graphicsCommand.Program =
                                            *programs.ProgSkinnedReflectionMapped;
                                        LOGV(
                                            "%s skinned reflection mapped material",
                                            materialTypeString);
                                    } else {
                                        if (programs.ProgReflectionMapped == nullptr) {
                                            ALOGE_FAIL("No ProgReflectionMapped set");
                                        }
                                        modelSurface.surfaceDef.graphicsCommand.Program =
                                            *programs.ProgReflectionMapped;
                                        LOGV("%s reflection mapped material", materialTypeString);
                                    }
                                } else {
                                    // light mapped material
                                    if (skinned) {
                                        if (programs.ProgSkinnedLightMapped == nullptr) {
                                            ALOGE_FAIL("No ProgSkinnedLightMapped set");
                                        }
                                        modelSurface.surfaceDef.graphicsCommand.Program =
                                            *programs.ProgSkinnedLightMapped;
                                        LOGV(
                                            "%s skinned light mapped material", materialTypeString);
                                    } else {
                                        if (programs.ProgLightMapped == nullptr) {
                                            ALOGE_FAIL("No ProgLightMapped set");
                                        }
                                        modelSurface.surfaceDef.graphicsCommand.Program =
                                            *programs.ProgLightMapped;
                                        LOGV("%s light mapped material", materialTypeString);
                                    }
                                }
                            } else {
                                // diffuse only material
                                if (skinned) {
                                    if (programs.ProgSkinnedSingleTexture == nullptr) {
                                        ALOGE_FAIL("No ProgSkinnedSingleTexture set");
                                    }
                                    modelSurface.surfaceDef.graphicsCommand.Program =
                                        *programs.ProgSkinnedSingleTexture;
                                    LOGV("%s skinned diffuse only material", materialTypeString);
                                } else {
                                    if (programs.ProgSingleTexture == nullptr) {
                                        ALOGE_FAIL("No ProgSingleTexture set");
                                    }
                                    modelSurface.surfaceDef.graphicsCommand.Program =
                                        *programs.ProgSingleTexture;
                                    LOGV("%s diffuse only material", materialTypeString);
                                }
                            }
                        } else if (attribs.color.size() > 0) {
                            // vertex color material
                            if (skinned) {
                                if (programs.ProgSkinnedVertexColor == nullptr) {
                                    ALOGE_FAIL("No ProgSkinnedVertexColor set");
                                }
                                modelSurface.surfaceDef.graphicsCommand.Program =
                                    *programs.ProgSkinnedVertexColor;
                                LOGV("%s skinned vertex color material", materialTypeString);
                            } else {
                                if (programs.ProgVertexColor == nullptr) {
                                    ALOGE_FAIL("No ProgVertexColor set");
                                }
                                modelSurface.surfaceDef.graphicsCommand.Program =
                                    *programs.ProgVertexColor;
                                LOGV("%s vertex color material", materialTypeString);
                            }
                        } else {
                            // surface without texture or vertex colors
                            modelSurface.surfaceDef.graphicsCommand.Textures[0] = GlTexture();
                            if (skinned) {
                                if (programs.ProgSkinnedSingleTexture == nullptr) {
                                    ALOGE_FAIL("No ProgSkinnedSingleTexture set");
                                }
                                modelSurface.surfaceDef.graphicsCommand.Program =
                                    *programs.ProgSkinnedSingleTexture;
                                LOGV("%s skinned default texture material", materialTypeString);
                            } else {
                                if (programs.ProgSingleTexture == nullptr) {
                                    ALOGE_FAIL("No ProgSingleTexture set");
                                }
                                modelSurface.surfaceDef.graphicsCommand.Program =
                                    *programs.ProgSingleTexture;
                                LOGV("%s default texture material", materialTypeString);
                            }
                        }

                        if (materialParms.PolygonOffset) {
                            modelSurface.surfaceDef.graphicsCommand.GpuState.polygonOffsetEnable =
                                true;
                            LOGV("polygon offset material");
                        }

                        modelFile.Models[modelIndex].surfaces.push_back(modelSurface);
                    }
                }
            }
        }

        //
        // Collision Model
        //

        const OVR::JsonReader collision_model(models.GetChildByName("collision_model"));
        if (collision_model.IsArray()) {
            LOGV("loading collision model..");

            while (!collision_model.IsEndOfArray()) {
                const size_t index = static_cast<size_t>(modelFile.Collisions.Polytopes.size());
                modelFile.Collisions.Polytopes.emplace_back(CollisionPolytope());

                const OVR::JsonReader polytope(collision_model.GetNextArrayElement());
                if (polytope.IsObject()) {
                    modelFile.Collisions.Polytopes[index].Name =
                        polytope.GetChildStringByName("name");
                    OVR::StringUtils::StringTo(
                        modelFile.Collisions.Polytopes[index].Planes,
                        polytope.GetChildStringByName("planes").c_str());
                }
            }
        }

        //
        // Ground Collision Model
        //

        const OVR::JsonReader ground_collision_model(
            models.GetChildByName("ground_collision_model"));
        if (ground_collision_model.IsArray()) {
            LOGV("loading ground collision model..");

            while (!ground_collision_model.IsEndOfArray()) {
                const size_t index =
                    static_cast<size_t>(modelFile.GroundCollisions.Polytopes.size());
                modelFile.GroundCollisions.Polytopes.emplace_back(CollisionPolytope());

                const OVR::JsonReader polytope(ground_collision_model.GetNextArrayElement());
                if (polytope.IsObject()) {
                    modelFile.GroundCollisions.Polytopes[index].Name =
                        polytope.GetChildStringByName("name");
                    OVR::StringUtils::StringTo(
                        modelFile.GroundCollisions.Polytopes[index].Planes,
                        polytope.GetChildStringByName("planes").c_str());
                }
            }
        }

        //
        // Ray-Trace Model
        //

        const OVR::JsonReader raytrace_model(models.GetChildByName("raytrace_model"));
        if (raytrace_model.IsObject()) {
            LOGV("loading ray-trace model..");

            ModelTrace& traceModel = modelFile.TraceModel;

            traceModel.header.numVertices = raytrace_model.GetChildInt32ByName("numVertices");
            traceModel.header.numUvs = raytrace_model.GetChildInt32ByName("numUvs");
            traceModel.header.numIndices = raytrace_model.GetChildInt32ByName("numIndices");
            traceModel.header.numNodes = raytrace_model.GetChildInt32ByName("numNodes");
            traceModel.header.numLeafs = raytrace_model.GetChildInt32ByName("numLeafs");
            traceModel.header.numOverflow = raytrace_model.GetChildInt32ByName("numOverflow");
            if (!traceModel.Validate(true)) {
                // this is a fatal error so that a model file from an untrusted source is never able
                // to cause out-of-bounds reads.
                ALOGE_FAIL("Invalid model data");
            }

            OVR::StringUtils::StringTo(
                traceModel.header.bounds, raytrace_model.GetChildStringByName("bounds").c_str());

            ReadModelArray(
                traceModel.vertices,
                raytrace_model.GetChildStringByName("vertices").c_str(),
                bin,
                traceModel.header.numVertices);
            ReadModelArray(
                traceModel.uvs,
                raytrace_model.GetChildStringByName("uvs").c_str(),
                bin,
                traceModel.header.numUvs);
            ReadModelArray(
                traceModel.indices,
                raytrace_model.GetChildStringByName("indices").c_str(),
                bin,
                traceModel.header.numIndices);

            if (!bin.ReadArray(traceModel.nodes, traceModel.header.numNodes)) {
                const OVR::JsonReader nodes_array(raytrace_model.GetChildByName("nodes"));
                if (nodes_array.IsArray()) {
                    while (!nodes_array.IsEndOfArray()) {
                        const size_t index = static_cast<size_t>(traceModel.nodes.size());
                        traceModel.nodes.emplace_back(kdtree_node_t());

                        const OVR::JsonReader node(nodes_array.GetNextArrayElement());
                        if (node.IsObject()) {
                            traceModel.nodes[index].data =
                                (std::uint32_t)node.GetChildInt64ByName("data");
                            traceModel.nodes[index].dist = node.GetChildFloatByName("dist");
                        }
                    }
                }
            }

            if (!bin.ReadArray(traceModel.leafs, traceModel.header.numLeafs)) {
                const OVR::JsonReader leafs_array(raytrace_model.GetChildByName("leafs"));
                if (leafs_array.IsArray()) {
                    while (!leafs_array.IsEndOfArray()) {
                        const size_t index = static_cast<size_t>(traceModel.leafs.size());
                        traceModel.leafs.emplace_back(kdtree_leaf_t());

                        const OVR::JsonReader leaf(leafs_array.GetNextArrayElement());
                        if (leaf.IsObject()) {
                            OVR::StringUtils::StringTo(
                                traceModel.leafs[index].triangles,
                                RT_KDTREE_MAX_LEAF_TRIANGLES,
                                leaf.GetChildStringByName("triangles").c_str());
                            OVR::StringUtils::StringTo(
                                traceModel.leafs[index].ropes,
                                6,
                                leaf.GetChildStringByName("ropes").c_str());
                            OVR::StringUtils::StringTo(
                                traceModel.leafs[index].bounds,
                                leaf.GetChildStringByName("bounds").c_str());
                        }
                    }
                }
            }

            ReadModelArray(
                traceModel.overflow,
                raytrace_model.GetChildStringByName("overflow").c_str(),
                bin,
                traceModel.header.numOverflow);
        }
    }

    if (!bin.IsAtEnd()) {
        ALOGW("failed to properly read binary file");
    }

    return true;
}

bool LoadModelFile_OvrScene(
    ModelFile* modelPtr,
    unzFile zfp,
    const char* fileName,
    const char* fileData,
    const int fileDataLength,
    const ModelGlPrograms& programs,
    const MaterialParms& materialParms,
    ModelGeo* outModelGeo) {
    // LOGCPUTIME( "LoadModelFile_OvrScene" );

    ModelFile& model = *modelPtr;

    if (!zfp) {
        ALOGW("Error: can't load %s", fileName);
        return false;
    }

    // load all texture files and locate the model files

    const char* modelsJson = nullptr;
    int modelsJsonLength = 0;

    const char* modelsBin = nullptr;
    int modelsBinLength = 0;

    for (int ret = unzGoToFirstFile(zfp); ret == UNZ_OK; ret = unzGoToNextFile(zfp)) {
        unz_file_info finfo;
        char entryName[256];
        unzGetCurrentFileInfo(zfp, &finfo, entryName, sizeof(entryName), nullptr, 0, nullptr, 0);
        LOGV("zip level: %ld, file: %s", finfo.compression_method, entryName);

        if (unzOpenCurrentFile(zfp) != UNZ_OK) {
            ALOGW("Failed to open %s from %s", entryName, fileName);
            continue;
        }

        const int size = finfo.uncompressed_size;
        char* buffer = nullptr;

        if (finfo.compression_method == 0 && fileData != nullptr) {
            buffer = (char*)fileData + unzGetCurrentFileZStreamPos64(zfp);
        } else {
            buffer = new char[size + 1];
            buffer[size] = '\0'; // always zero terminate text files

            if (unzReadCurrentFile(zfp, buffer, size) != size) {
                ALOGW("Failed to read %s from %s", entryName, fileName);
                delete[] buffer;
                continue;
            }
        }

        // assume a 3 character extension
        const size_t entryLength = strlen(entryName);
        const char* extension = (entryLength >= 4) ? &entryName[entryLength - 4] : entryName;

        if (OVR::OVR_stricmp(entryName, "models.json") == 0) {
            // save this for parsing
            modelsJson = (const char*)buffer;
            modelsJsonLength = size;
            buffer = nullptr; // don't free it now
        } else if (OVR::OVR_stricmp(entryName, "models.bin") == 0) {
            // save this for parsing
            modelsBin = (const char*)buffer;
            modelsBinLength = size;
            buffer = nullptr; // don't free it now
        } else if (
            OVR::OVR_stricmp(extension, ".pvr") == 0 || OVR::OVR_stricmp(extension, ".ktx") == 0) {
            // only support .pvr and .ktx containers for now
            LoadModelFileTexture(model, entryName, buffer, size, materialParms);
        } else {
            // ignore other files
            LOGV("Ignoring %s", entryName);
        }

        if (buffer < fileData || buffer > fileData + fileDataLength) {
            delete[] buffer;
        }

        unzCloseCurrentFile(zfp);
    }
    unzClose(zfp);

    bool loaded = false;

    if (modelsJson != nullptr) {
        loaded = LoadModelFile_OvrScene_Json(
            model,
            modelsJson,
            modelsJsonLength,
            modelsBin,
            modelsBinLength,
            programs,
            materialParms,
            outModelGeo);
    }

    if (modelsJson < fileData || modelsJson > fileData + fileDataLength) {
        delete modelsJson;
    }
    if (modelsBin < fileData || modelsBin > fileData + fileDataLength) {
        delete modelsBin;
    }

    return loaded;
}

} // namespace OVRFW
