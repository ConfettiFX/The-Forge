/* Write your header comments here */
#include <metal_stdlib>
#include <metal_compute>
using namespace metal;

struct Compute_Shader
{
    texture2d<float> DepthTexture;
    texture3d<half> SDFVolumeTextureAtlas;
    texture2d<float, access::read_write> OutTexture;
    sampler clampToEdgeTrillinearSampler;
    sampler clampToEdgeNearSampler;
    struct Uniforms_cameraUniformBlock
    {
        float4x4 View;
        float4x4 Project;
        float4x4 ViewProject;
        float4x4 InvView;
        float4x4 InvProj;
        float4x4 InvViewProject;
        float4  mCameraPos;
        float mNear;
        float mFarNearDiff;
        float mFarNear;
        float paddingForAlignment0;
        float2 mTwoOverRes;
        float _pad1;
        float _pad2;
        float2 mWindowSize;
    };
    constant Uniforms_cameraUniformBlock & cameraUniformBlock;
    struct Uniforms_meshSDFUniformBlock
    {
        float4x4 mWorldToVolumeMat[256];
        float4 mUVScaleAndVolumeScale[256];
        float4 mLocalPositionExtent[256];
        float4 mUVAddAndSelfShadowBias[256];
        float4 mSDFMAD[256];
        uint mNumObjects;
    };
    constant Uniforms_meshSDFUniformBlock & meshSDFUniformBlock;
    float2 LineBoxIntersect(float3 RayOrigin, float3 RayEnd, float3 BoxMin, float3 BoxMax)
    {
        float3 InvRayDir = ((float3)(1.0) / (RayEnd - RayOrigin));
        float3 FirstPlaneIntersections = ((BoxMin - RayOrigin) * InvRayDir);
        float3 SecondPlaneIntersections = ((BoxMax - RayOrigin) * InvRayDir);
        float3 ClosestPlaneIntersections = min((float3)FirstPlaneIntersections,(float3)SecondPlaneIntersections);
        float3 FurthestPlaneIntersections = max((float3)FirstPlaneIntersections,(float3)SecondPlaneIntersections);
        float2 BoxIntersections;
        ((BoxIntersections).x = max((float)ClosestPlaneIntersections.x,(float)max((float)ClosestPlaneIntersections.y,(float)ClosestPlaneIntersections.z)));
        ((BoxIntersections).y = min((float)FurthestPlaneIntersections.x,(float)min((float)FurthestPlaneIntersections.y,(float)FurthestPlaneIntersections.z)));
        return saturate(BoxIntersections);
    };
    float3 SDFVolumeDimensionPositionToUV(float3 volumePos, float3 uvScale, float3 uvAdd)
    {
        return ((volumePos * uvScale) + uvAdd);
    };
    float SampleSDFVolumeTextureAtlas(float3 volumeUV)
    {
        return SDFVolumeTextureAtlas.sample(clampToEdgeTrillinearSampler, volumeUV, level(0)).r;
    };
    void RayTraceScene(float3 rayWorldStartPos, float3 rayWorldEndPos, float maxRayTime, thread float(& minRayTime), thread float(& stepsTaken))
    {
        (minRayTime = maxRayTime);
        (stepsTaken = (float)(0));
        for (uint index = (uint)(0); (index < meshSDFUniformBlock.mNumObjects); (++index))
        {
            float3 volumeRayStart = (((meshSDFUniformBlock.mWorldToVolumeMat[index])*(float4((rayWorldStartPos).xyz, 1.0)))).xyz;
            float3 volumeRayEnd = (((meshSDFUniformBlock.mWorldToVolumeMat[index])*(float4((rayWorldEndPos).xyz, 1.0)))).xyz;
            float3 volumeRayDir = (volumeRayEnd - volumeRayStart);
            float volumeRayLength = length(volumeRayDir);
            (volumeRayDir /= (float3)(volumeRayLength));
            float2 intersectionTimes = LineBoxIntersect(volumeRayStart, volumeRayEnd, (-(meshSDFUniformBlock.mLocalPositionExtent[index]).xyz), (meshSDFUniformBlock.mLocalPositionExtent[index]).xyz);
            if ((((intersectionTimes).x < (intersectionTimes).y) && ((intersectionTimes).x < (float)(1))))
            {
                float sampleRayTime = ((intersectionTimes).x * volumeRayLength);
                float minDist = (float)(1000000);
                uint stepIndex = (uint)(0);
                uint maxSteps = (uint)(256);
                for (; (stepIndex < maxSteps); (++stepIndex))
                {
                    float3 sampleVolumePos = (volumeRayStart + (volumeRayDir * (float3)(sampleRayTime)));
                    float3 clampedSamplePos = clamp(sampleVolumePos, (-(meshSDFUniformBlock.mLocalPositionExtent[index]).xyz), (meshSDFUniformBlock.mLocalPositionExtent[index]).xyz);
                    float3 volumeUV = SDFVolumeDimensionPositionToUV(clampedSamplePos, (meshSDFUniformBlock.mUVScaleAndVolumeScale[index]).xyz, (meshSDFUniformBlock.mUVAddAndSelfShadowBias[index]).xyz);
                    float sdfValue = SampleSDFVolumeTextureAtlas(volumeUV);
                    (minDist = min((float)minDist,(float)sdfValue));
                    float minStepSize = (1.0 / (4.0 * (float)(maxSteps)));
                    float curStepDist = max((float)sdfValue,(float)minStepSize);
                    (sampleRayTime += curStepDist);
                    if (((sdfValue < (float)(0)) || (sampleRayTime > ((intersectionTimes).y * volumeRayLength))))
                    {
                        break;
                    }
                }
                if ((((minDist * (meshSDFUniformBlock.mUVScaleAndVolumeScale[index]).w) < (float)(0)) || (stepIndex == maxSteps)))
                {
                    (minRayTime = min((float)minRayTime,(float)sampleRayTime * (meshSDFUniformBlock.mUVScaleAndVolumeScale[index]).w));
                }
                (stepsTaken += (float)(stepIndex));
            }
        }
    };
    void main(uint3 GroupID, uint3 DispatchThreadID, uint3 GroupThreadID)
    {
        uint3 localDispatchThreadID = (DispatchThreadID * (uint3)(2));
        float xClip = (((float((localDispatchThreadID).x) * 2.0) / (cameraUniformBlock.mWindowSize).x) - 1.0);
        float yClip = (1.0 - ((float((localDispatchThreadID).y) * 2.0) / (cameraUniformBlock.mWindowSize).y));
        float2 depthSize = cameraUniformBlock.mWindowSize;
        float2 depthUV = ((float2)((localDispatchThreadID).xy) / depthSize);
        float depthVal = (float)(DepthTexture.sample(clampToEdgeNearSampler, depthUV, level(0)).r);
        float4 worldPosW = ((cameraUniformBlock.InvViewProject)*(float4(xClip, yClip, depthVal, 1.0)));
        float3 worldPos = ((worldPosW / (float4)((worldPosW).w))).xyz;
        float traceDistance = (float)(40000);
        float3 cameraWorldPos = (cameraUniformBlock.InvView[3]).xyz;
        float3 rayDir = normalize((worldPos - cameraWorldPos));
        float3 rayEndPos = (cameraWorldPos + (rayDir * (float3)(traceDistance)));
        float minRayTime = traceDistance;
        float totalStepsTaken = (float)(0);
        RayTraceScene(cameraWorldPos, rayEndPos, traceDistance, minRayTime, totalStepsTaken);
        float saturatedStepsTaken = saturate((totalStepsTaken / 200.0));
        float3 Result = float3(saturatedStepsTaken, saturatedStepsTaken, saturatedStepsTaken);
        if ((minRayTime < traceDistance))
        {
            (Result += (float3)(0.1));
        }
        (OutTexture.write(float4(Result, 1.0), uint2((DispatchThreadID).xy)));
    };

    Compute_Shader(
texture2d<float> DepthTexture,texture3d<half> SDFVolumeTextureAtlas,texture2d<float, access::read_write> OutTexture,sampler clampToEdgeTrillinearSampler,sampler clampToEdgeNearSampler,constant Uniforms_cameraUniformBlock & cameraUniformBlock,constant Uniforms_meshSDFUniformBlock & meshSDFUniformBlock) :
DepthTexture(DepthTexture),SDFVolumeTextureAtlas(SDFVolumeTextureAtlas),OutTexture(OutTexture),clampToEdgeTrillinearSampler(clampToEdgeTrillinearSampler),clampToEdgeNearSampler(clampToEdgeNearSampler),cameraUniformBlock(cameraUniformBlock),meshSDFUniformBlock(meshSDFUniformBlock) {}
};

struct CSData {
    texture2d<float> DepthTexture                   [[id(0)]];
    texture3d<half> SDFVolumeTextureAtlas           [[id(1)]];
    texture2d<float, access::read_write> OutTexture [[id(2)]];
    sampler clampToEdgeTrillinearSampler            [[id(3)]];
    sampler clampToEdgeNearSampler                  [[id(4)]];
};

struct CSDataPerFrame {
    constant Compute_Shader::Uniforms_cameraUniformBlock& cameraUniformBlock       [[id(0)]];
    constant Compute_Shader::Uniforms_meshSDFUniformBlock& meshSDFUniformBlock     [[id(1)]];
};

//[numthreads(16, 16, 1)]
kernel void stageMain(
    uint3 GroupID                               [[threadgroup_position_in_grid]],
    uint3 DispatchThreadID                      [[thread_position_in_grid]],
    uint3 GroupThreadID                         [[thread_position_in_threadgroup]],
    constant CSData& csData                     [[buffer(UPDATE_FREQ_NONE)]],
    constant CSDataPerFrame& csDataPerFrame     [[buffer(UPDATE_FREQ_PER_FRAME)]]
)
{
    uint3 GroupID0;
    GroupID0 = GroupID;
    uint3 DispatchThreadID0;
    DispatchThreadID0 = DispatchThreadID;
    uint3 GroupThreadID0;
    GroupThreadID0 = GroupThreadID;
    Compute_Shader main(csData.DepthTexture, csData.SDFVolumeTextureAtlas, csData.OutTexture, csData.clampToEdgeTrillinearSampler, csData.clampToEdgeNearSampler, csDataPerFrame.cameraUniformBlock, csDataPerFrame.meshSDFUniformBlock);
    return main.main(GroupID0, DispatchThreadID0, GroupThreadID0);
}
