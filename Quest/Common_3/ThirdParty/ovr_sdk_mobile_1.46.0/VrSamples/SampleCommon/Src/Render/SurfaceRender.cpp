/************************************************************************************

Filename    :   SurfaceRender.cpp
Content     :   Optimized OpenGL rendering path
Created     :   August 9, 2013
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/

#include "SurfaceRender.h"

#include <stdlib.h>

#include "Misc/Log.h"

#include "Egl.h"
#include "GlTexture.h"
#include "GlProgram.h"
#include "GlBuffer.h"

#include <algorithm>

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

bool LogRenderSurfaces = false; // Do not check in set to true!

static void
ChangeGpuState(const ovrGpuState oldState, const ovrGpuState newState, bool force = false) {
    if (force || newState.blendEnable != oldState.blendEnable) {
        if (newState.blendEnable) {
            GL(glEnable(GL_BLEND));
        } else {
            GL(glDisable(GL_BLEND));
        }
    }
    if (force || newState.blendEnable != oldState.blendEnable ||
        newState.blendSrc != oldState.blendSrc || newState.blendDst != oldState.blendDst ||
        newState.blendSrcAlpha != oldState.blendSrcAlpha ||
        newState.blendDstAlpha != oldState.blendDstAlpha ||
        newState.blendMode != oldState.blendMode ||
        newState.blendModeAlpha != oldState.blendModeAlpha) {
        if (newState.blendEnable == ovrGpuState::BLEND_ENABLE_SEPARATE) {
            GL(glBlendFuncSeparate(
                newState.blendSrc,
                newState.blendDst,
                newState.blendSrcAlpha,
                newState.blendDstAlpha));
            GL(glBlendEquationSeparate(newState.blendMode, newState.blendModeAlpha));
        } else {
            GL(glBlendFunc(newState.blendSrc, newState.blendDst));
            GL(glBlendEquation(newState.blendMode));
        }
    }

    if (force || newState.depthFunc != oldState.depthFunc) {
        GL(glDepthFunc(newState.depthFunc));
    }
    if (force || newState.frontFace != oldState.frontFace) {
        GL(glFrontFace(newState.frontFace));
    }
    if (force || newState.depthEnable != oldState.depthEnable) {
        if (newState.depthEnable) {
            GL(glEnable(GL_DEPTH_TEST));
        } else {
            GL(glDisable(GL_DEPTH_TEST));
        }
    }
    if (force || newState.depthMaskEnable != oldState.depthMaskEnable) {
        if (newState.depthMaskEnable) {
            GL(glDepthMask(GL_TRUE));
        } else {
            GL(glDepthMask(GL_FALSE));
        }
    }
    if (force || newState.colorMaskEnable[0] != oldState.colorMaskEnable[0] ||
        newState.colorMaskEnable[1] != oldState.colorMaskEnable[1] ||
        newState.colorMaskEnable[2] != oldState.colorMaskEnable[2] ||
        newState.colorMaskEnable[3] != oldState.colorMaskEnable[3]) {
        GL(glColorMask(
            newState.colorMaskEnable[0] ? GL_TRUE : GL_FALSE,
            newState.colorMaskEnable[1] ? GL_TRUE : GL_FALSE,
            newState.colorMaskEnable[2] ? GL_TRUE : GL_FALSE,
            newState.colorMaskEnable[3] ? GL_TRUE : GL_FALSE));
    }
    if (force || newState.polygonOffsetEnable != oldState.polygonOffsetEnable) {
        if (newState.polygonOffsetEnable) {
            GL(glEnable(GL_POLYGON_OFFSET_FILL));
            GL(glPolygonOffset(1.0f, 1.0f));
        } else {
            GL(glDisable(GL_POLYGON_OFFSET_FILL));
        }
    }
    if (force || newState.cullEnable != oldState.cullEnable) {
        if (newState.cullEnable) {
            GL(glEnable(GL_CULL_FACE));
        } else {
            GL(glDisable(GL_CULL_FACE));
        }
    }
    if (force || newState.lineWidth != oldState.lineWidth) {
        GL(glLineWidth(newState.lineWidth));
    }
    if (force || (newState.depthRange[0] != oldState.depthRange[0]) ||
        (newState.depthRange[1] != oldState.depthRange[1])) {
        GL(glDepthRangef(newState.depthRange[0], newState.depthRange[1]));
    }
#if GL_ES_VERSION_2_0 == 0
    if (force || newState.polygonMode != oldState.polygonMode) {
        GL(glPolygonMode(GL_FRONT_AND_BACK, newState.polygonMode));
    }
#endif
    // extend as needed
}

ovrSurfaceRender::ovrSurfaceRender() : CurrentSceneMatricesIdx(0) {}

ovrSurfaceRender::~ovrSurfaceRender() {}

void ovrSurfaceRender::Init() {
    for (int i = 0; i < MAX_SCENEMATRICES_UBOS; i++) {
        SceneMatrices[i].Create(GLBUFFER_TYPE_UNIFORM, GlProgram::SCENE_MATRICES_UBO_SIZE, NULL);
    }

    CurrentSceneMatricesIdx = 0;
}

void ovrSurfaceRender::Shutdown() {
    for (int i = 0; i < MAX_SCENEMATRICES_UBOS; i++) {
        SceneMatrices[i].Destroy();
    }
}

int ovrSurfaceRender::UpdateSceneMatrices(
    const Matrix4f* viewMatrix,
    const Matrix4f* projectionMatrix,
    const int numViews) {
    assert(numViews >= 0 && numViews <= GlProgram::MAX_VIEWS);

    // ----DEPRECATED_DRAWEYEVIEW
    // NOTE: Apps which still use DrawEyeView (or that are in process of moving away from it) will
    // call RenderSurfaceList multiple times per frame outside of AppRender. This can cause a
    // rendering hazard in that we may be updating the scene matrices ubo while it is still in use.
    // The typical DEV case is 2x a frame, but have seen it top 8x a frame. Since the matrices
    // in the DEV path are typically the same, test for that condition and don't update the ubo.
    // This check can be removed once the DEV path is removed.
    bool requiresUpdate = false;
#if 1
    for (int i = 0; i < numViews; i++) {
        requiresUpdate |= !(CachedViewMatrix[i] == viewMatrix[i]);
        requiresUpdate |= !(CachedProjectionMatrix[i] == projectionMatrix[i]);
    }
#else
    requiresUpdate = true;
#endif
    // ----DEPRECATED_DRAWEYEVIEW

    if (requiresUpdate) {
        // Advance to the next available scene matrix ubo.
        CurrentSceneMatricesIdx = (CurrentSceneMatricesIdx + 1) % MAX_SCENEMATRICES_UBOS;

        // Cache and transpose the matrices before passing to GL.
        Matrix4f viewMatrixTransposed[GlProgram::MAX_VIEWS];
        Matrix4f projectionMatrixTransposed[GlProgram::MAX_VIEWS];
        for (int i = 0; i < numViews; i++) {
            CachedViewMatrix[i] = viewMatrix[i];
            CachedProjectionMatrix[i] = projectionMatrix[i];

            viewMatrixTransposed[i] = CachedViewMatrix[i].Transposed();
            projectionMatrixTransposed[i] = CachedProjectionMatrix[i].Transposed();
        }

        void* matricesBuffer = SceneMatrices[CurrentSceneMatricesIdx].MapBuffer();
        if (matricesBuffer != NULL) {
            memcpy(
                (char*)matricesBuffer + 0 * GlProgram::MAX_VIEWS * sizeof(Matrix4f),
                viewMatrixTransposed,
                GlProgram::MAX_VIEWS * sizeof(Matrix4f));
            memcpy(
                (char*)matricesBuffer + 1 * GlProgram::MAX_VIEWS * sizeof(Matrix4f),
                projectionMatrixTransposed,
                GlProgram::MAX_VIEWS * sizeof(Matrix4f));
            SceneMatrices[CurrentSceneMatricesIdx].UnmapBuffer();
        }
    }

    // ALOG( "UpdateSceneMatrices: RequiresUpdate %d, CurrIdx %d", requiresUpdate,
    // CurrentSceneMatricesIdx );

    return CurrentSceneMatricesIdx;
}

// Renders a list of pointers to models in order.
ovrDrawCounters ovrSurfaceRender::RenderSurfaceList(
    const std::vector<ovrDrawSurface>& surfaceList,
    const Matrix4f& viewMatrix,
    const Matrix4f& projectionMatrix,
    const int eye) {
    assert(eye >= 0 && eye < GlProgram::MAX_VIEWS);

    // Force the GPU state to a known value, then only set on changes
    ovrGpuState currentGpuState;
    ChangeGpuState(currentGpuState, currentGpuState, true /* force */);

    // TODO: These should be range checked containers.
    GLuint currentBuffers[ovrUniform::MAX_UNIFORMS] = {};
    GLuint currentTextures[ovrUniform::MAX_UNIFORMS] = {};
    GLuint currentProgramObject = 0;

    const int sceneMatricesIdx =
        UpdateSceneMatrices(&viewMatrix, &projectionMatrix, GlProgram::MAX_VIEWS /* num eyes */);

    // counters
    ovrDrawCounters counters;

    // Loop through all the surfaces
    for (const ovrDrawSurface& drawSurface : surfaceList) {
        const ovrSurfaceDef& surfaceDef = *drawSurface.surface;
        const ovrGraphicsCommand& cmd = surfaceDef.graphicsCommand;

        if (cmd.Program.IsValid()) {
            ChangeGpuState(currentGpuState, cmd.GpuState);
            currentGpuState = cmd.GpuState;
            GLCheckErrorsWithTitle(surfaceDef.surfaceName.c_str());

            // update the program object
            if (cmd.Program.Program != currentProgramObject) {
                counters.numProgramBinds++;

                currentProgramObject = cmd.Program.Program;
                GL(glUseProgram(cmd.Program.Program));
            }

            // Update globally defined system level uniforms.
            {
                if (cmd.Program.ViewID.Location >= 0) // not defined when multiview enabled
                {
                    GL(glUniform1i(cmd.Program.ViewID.Location, eye));
                }
                GL(glUniformMatrix4fv(
                    cmd.Program.ModelMatrix.Location, 1, GL_TRUE, drawSurface.modelMatrix.M[0]));

                if (cmd.Program.SceneMatrices.Location >= 0) {
                    GL(glBindBufferBase(
                        GL_UNIFORM_BUFFER,
                        cmd.Program.SceneMatrices.Binding,
                        SceneMatrices[sceneMatricesIdx].GetBuffer()));
                }
            }

            // update texture bindings and uniform values
            bool uniformsDone = false;
            {
                for (int i = 0; i < ovrUniform::MAX_UNIFORMS && !uniformsDone; ++i) {
                    counters.numParameterUpdates++;
                    const int parmLocation = cmd.Program.Uniforms[i].Location;

                    switch (cmd.Program.Uniforms[i].Type) {
                        case ovrProgramParmType::INT: {
                            if (parmLocation >= 0 && cmd.UniformData[i].Data != NULL) {
                                GL(glUniform1iv(
                                    parmLocation,
                                    1,
                                    static_cast<const int*>(cmd.UniformData[i].Data)));
                            }
                        } break;
                        case ovrProgramParmType::INT_VECTOR2: {
                            if (parmLocation >= 0 && cmd.UniformData[i].Data != NULL) {
                                GL(glUniform2iv(
                                    parmLocation,
                                    1,
                                    static_cast<const int*>(cmd.UniformData[i].Data)));
                            }
                        } break;
                        case ovrProgramParmType::INT_VECTOR3: {
                            if (parmLocation >= 0 && cmd.UniformData[i].Data != NULL) {
                                GL(glUniform3iv(
                                    parmLocation,
                                    1,
                                    static_cast<const int*>(cmd.UniformData[i].Data)));
                            }
                        } break;
                        case ovrProgramParmType::INT_VECTOR4: {
                            if (parmLocation >= 0 && cmd.UniformData[i].Data != NULL) {
                                GL(glUniform4iv(
                                    parmLocation,
                                    1,
                                    static_cast<const int*>(cmd.UniformData[i].Data)));
                            }
                        } break;
                        case ovrProgramParmType::FLOAT: {
                            if (parmLocation >= 0 && cmd.UniformData[i].Data != NULL) {
                                GL(glUniform1f(
                                    parmLocation,
                                    *static_cast<const float*>(cmd.UniformData[i].Data)));
                            }
                        } break;
                        case ovrProgramParmType::FLOAT_VECTOR2: {
                            if (parmLocation >= 0 && cmd.UniformData[i].Data != NULL) {
                                GL(glUniform2fv(
                                    parmLocation,
                                    1,
                                    static_cast<const float*>(cmd.UniformData[i].Data)));
                            }
                        } break;
                        case ovrProgramParmType::FLOAT_VECTOR3: {
                            if (parmLocation >= 0 && cmd.UniformData[i].Data != NULL) {
                                GL(glUniform3fv(
                                    parmLocation,
                                    1,
                                    static_cast<const float*>(cmd.UniformData[i].Data)));
                            }
                        } break;
                        case ovrProgramParmType::FLOAT_VECTOR4: {
                            if (parmLocation >= 0 && cmd.UniformData[i].Data != NULL) {
                                GL(glUniform4fv(
                                    parmLocation,
                                    1,
                                    static_cast<const float*>(cmd.UniformData[i].Data)));
                            }
                        } break;
                        case ovrProgramParmType::FLOAT_MATRIX4: {
                            if (parmLocation >= 0 && cmd.UniformData[i].Data != NULL) {
                                if (cmd.UniformData[i].Count > 1) {
                                    /// FIXME: setting glUniformMatrix4fv transpose to GL_TRUE for
                                    /// an array of matrices produces garbage using the Adreno 420
                                    /// OpenGL ES 3.0 driver.
                                    static Matrix4f transposedJoints[MAX_JOINTS];
                                    const int numJoints =
                                        std::min<int>(cmd.UniformData[i].Count, MAX_JOINTS);
                                    for (int j = 0; j < numJoints; j++) {
                                        transposedJoints[j] =
                                            static_cast<Matrix4f*>(cmd.UniformData[i].Data)[j]
                                                .Transposed();
                                    }
                                    GL(glUniformMatrix4fv(
                                        parmLocation,
                                        numJoints,
                                        GL_FALSE,
                                        static_cast<const float*>(&transposedJoints[0].M[0][0])));
                                } else {
                                    GL(glUniformMatrix4fv(
                                        parmLocation,
                                        cmd.UniformData[i].Count,
                                        GL_TRUE,
                                        static_cast<const float*>(cmd.UniformData[i].Data)));
                                }
                            }
                        } break;
                        case ovrProgramParmType::TEXTURE_SAMPLED: {
                            const int parmBinding = cmd.Program.Uniforms[i].Binding;
                            if (parmBinding >= 0 && cmd.UniformData[i].Data != NULL) {
                                const GlTexture& texture =
                                    *static_cast<GlTexture*>(cmd.UniformData[i].Data);
                                if (currentTextures[parmBinding] != texture.texture) {
                                    counters.numTextureBinds++;
                                    currentTextures[parmBinding] = texture.texture;
                                    GL(glActiveTexture(GL_TEXTURE0 + parmBinding));
                                    GL(glBindTexture(
                                        texture.target ? texture.target : GL_TEXTURE_2D,
                                        texture.texture));
                                }
                            }
                        } break;
                        case ovrProgramParmType::BUFFER_UNIFORM: {
                            const int parmBinding = cmd.Program.Uniforms[i].Binding;
                            if (parmBinding >= 0 && cmd.UniformData[i].Data != NULL) {
                                const GlBuffer& buffer =
                                    *static_cast<GlBuffer*>(cmd.UniformData[i].Data);
                                if (currentBuffers[parmBinding] != buffer.GetBuffer()) {
                                    counters.numBufferBinds++;
                                    currentBuffers[parmBinding] = buffer.GetBuffer();
                                    GL(glBindBufferBase(
                                        GL_UNIFORM_BUFFER, parmBinding, buffer.GetBuffer()));
                                }
                            }
                        } break;
                        case ovrProgramParmType::MAX:
                            uniformsDone = true;
                            break; // done
                        default:
                            assert(false);
                            uniformsDone = true;
                            break;
                    }
                }
            }
        }

        counters.numDrawCalls++;

        if (LogRenderSurfaces) {
            ALOG(
                "Drawing %s vao=%d vb=%d primitive=0x%04x indexCount=%d IndexType=0x%04x ",
                surfaceDef.surfaceName.c_str(),
                surfaceDef.geo.vertexArrayObject,
                surfaceDef.geo.vertexBuffer,
                surfaceDef.geo.primitiveType,
                surfaceDef.geo.indexCount,
                surfaceDef.geo.IndexType);
        }

        // Bind all the vertex and element arrays
        {
            GL(glBindVertexArray(surfaceDef.geo.vertexArrayObject));

            if (surfaceDef.numInstances > 1) {
                GL(glDrawElementsInstanced(
                    surfaceDef.geo.primitiveType,
                    surfaceDef.geo.indexCount,
                    surfaceDef.geo.IndexType,
                    NULL,
                    surfaceDef.numInstances));
            } else {
                GL(glDrawElements(
                    surfaceDef.geo.primitiveType,
                    surfaceDef.geo.indexCount,
                    surfaceDef.geo.IndexType,
                    NULL));
            }
        }

        GLCheckErrorsWithTitle(surfaceDef.surfaceName.c_str());
    }

    // set the gpu state back to the default
    ChangeGpuState(currentGpuState, ovrGpuState());
    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, 0));
    GL(glUseProgram(0));
    GL(glBindVertexArray(0));

    return counters;
}

} // namespace OVRFW
