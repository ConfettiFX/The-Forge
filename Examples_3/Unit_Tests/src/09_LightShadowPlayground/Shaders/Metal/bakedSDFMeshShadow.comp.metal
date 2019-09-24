/* Write your header comments here */
#include <metal_stdlib>
#include <metal_compute>
using namespace metal;

struct Compute_Shader
{
    depth2d<float, access::sample> DepthTexture;
    texture3d<half> SDFVolumeTextureAtlas;
    texture2d<float, access::write> OutTexture;
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
        packed_float4  mCameraPos;
        float mNear;
        float mFarNearDiff;
        float mFarNear;
        float paddingForAlignment0;
        packed_float2 mTwoOverRes;
        float _pad1;
        float _pad2;
        packed_float2 mWindowSize;
        float _pad3;
        float _pad4;
        packed_float4 mDeviceZToWorldZ;
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
    struct Uniforms_lightUniformBlock
    {
        float4x4 lightViewProj;
        float4 lightPosition;
        float4 lightColor;
        float4 mLightUpVec;
        float4 mTanLightAngleAndThresholdValue;
        float3 mLightDir;
    };
    constant Uniforms_lightUniformBlock & lightUniformBlock;
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
    float ShadowRayMarch(float3 rayWorldStartPos, float3 rayWorldEndPos, float tanLightAngle, float minSphereRadius, float maxSphereRadius)
    {
        const float shadowZeroThereshold = 0.010000000;
        float minShadow = (float)(1.0);
        for (uint index = (uint)(0); (index < meshSDFUniformBlock.mNumObjects); (++index))
        {
            const float twoSidedMeshDistanceBias = (meshSDFUniformBlock.mSDFMAD[index]).z;
            float4x4 worldToVolumeMat = meshSDFUniformBlock.mWorldToVolumeMat[index];
            float3 volumeRayStart = (((worldToVolumeMat)*(float4((rayWorldStartPos).xyz, 1.0)))).xyz;
            float3 volumeRayEnd = (((worldToVolumeMat)*(float4((rayWorldEndPos).xyz, 1.0)))).xyz;
            float3 volumeRayDir = (volumeRayEnd - volumeRayStart);
            float volumeRayLength = length(volumeRayDir);
            (volumeRayDir /= (float3)(volumeRayLength));
            float4 uvScaleAndVolumeScale = meshSDFUniformBlock.mUVScaleAndVolumeScale[index];
            bool isTwoSided = ((((uvScaleAndVolumeScale).w < (float)(0.0)))?(true):(false));
            float finalVolumeScale = abs((uvScaleAndVolumeScale).w);
            float worldToVolumeScale = (1.0 / finalVolumeScale);
            float volumeMinSphereRadius = (minSphereRadius * worldToVolumeScale);
            float volumeMaxSphereRadius = (maxSphereRadius * worldToVolumeScale);
            float volumeTwoSidedMeshDistanceBias = (twoSidedMeshDistanceBias * worldToVolumeScale);
            float objectCenterDistAlongRay = dot((-volumeRayStart), volumeRayDir);
            float localConeRadiusAtObject = min((float)tanLightAngle * max((float)objectCenterDistAlongRay,(float)0),(float)volumeMaxSphereRadius);
            float3 localPositionExtent = (meshSDFUniformBlock.mLocalPositionExtent[index]).xyz;
            float2 intersectionTimes = LineBoxIntersect(volumeRayStart, volumeRayEnd, (((-(localPositionExtent).xyz) * (float3)(0.9)) - (float3)(localConeRadiusAtObject)), (((localPositionExtent).xyz * (float3)(0.9)) + (float3)(localConeRadiusAtObject)));
            if (((intersectionTimes).x < (intersectionTimes).y))
            {
                float4 uvAddAndSelfShadowBias = meshSDFUniformBlock.mUVAddAndSelfShadowBias[index];
                float selfShadowScale = (1.0 / max((float)uvAddAndSelfShadowBias.w * worldToVolumeScale,(float)0.00010000000));
                float sampleRayTime = ((intersectionTimes).x * volumeRayLength);
                uint stepIndex = (uint)(0);
                uint maxSteps = (uint)(64);
                float minStepSize = (1.0 / (4.0 * (float)(maxSteps)));
                for (; (stepIndex < maxSteps); (++stepIndex))
                {
                    float3 sampleVolumePos = (volumeRayStart + (volumeRayDir * (float3)(sampleRayTime)));
                    float3 clampedSamplePos = clamp(sampleVolumePos, (-(localPositionExtent).xyz), (localPositionExtent).xyz);
                    float distanceToClamped = length((clampedSamplePos - sampleVolumePos));
                    float3 volumeUV = SDFVolumeDimensionPositionToUV(clampedSamplePos, (uvScaleAndVolumeScale).xyz, (uvAddAndSelfShadowBias).xyz);
                    float sdfValue = (SampleSDFVolumeTextureAtlas(volumeUV) + distanceToClamped);
                    if (isTwoSided)
                    {
                        (sdfValue -= volumeTwoSidedMeshDistanceBias);
                    }
                    float selfShadowVisibility = (1.0 - saturate((sampleRayTime * selfShadowScale)));
                    float sphereRadius = clamp((tanLightAngle * sampleRayTime), volumeMinSphereRadius, volumeMaxSphereRadius);
                    float stepVisibility = max((float)saturate((sdfValue / sphereRadius)),(float)selfShadowVisibility);
                    (minShadow = min((float)minShadow,(float)stepVisibility));
                    float nextStepIncrement = abs(sdfValue);
                    (nextStepIncrement = ((nextStepIncrement * 0.1) + 0.010000000));
                    float curStepDist = max((float)nextStepIncrement,(float)minStepSize);
                    (sampleRayTime += curStepDist);
                    if (((minShadow < shadowZeroThereshold) || (sampleRayTime > ((intersectionTimes).y * volumeRayLength))))
                    {
                        break;
                    }
                }
            }
            if ((minShadow < shadowZeroThereshold))
            {
                (minShadow = 0.0);
                break;
            }
        }
        return minShadow;
    };
    float ConvertFromDeviceZ(float deviceZ)
    {
        return (((deviceZ * cameraUniformBlock.mDeviceZToWorldZ[0]) + cameraUniformBlock.mDeviceZToWorldZ[1]) + (1.0 / ((deviceZ * cameraUniformBlock.mDeviceZToWorldZ[2]) - cameraUniformBlock.mDeviceZToWorldZ[3])));
    };
	
    void main(uint3 GroupID, uint3 DispatchThreadID, uint3 GroupThreadID)
    {
        uint3 localDispatchThreadID = (DispatchThreadID * (uint3)(2));
        float xClip = (((float((localDispatchThreadID).x) * 2.0) / (cameraUniformBlock.mWindowSize).x) - 1.0);
        float yClip = (1.0 - ((float((localDispatchThreadID).y) * 2.0) / (cameraUniformBlock.mWindowSize).y));
        float2 depthSize = cameraUniformBlock.mWindowSize;
        float2 depthUV = float2((float)(localDispatchThreadID.x) / depthSize.x, (float)(localDispatchThreadID.y) / depthSize.y);
        float depthVal = (DepthTexture.sample(clampToEdgeNearSampler, depthUV, 0));
		
        float4 worldPosW = ((cameraUniformBlock.InvViewProject)*(float4(xClip, yClip, depthVal, 1.0)));
        float3 worldPos = ((worldPosW / (float4)((worldPosW).w))).xyz;
        float worldZ = ConvertFromDeviceZ(depthVal);
        float RayStartOffset = 1.75f + 0.008f * worldZ;
        float minSphereRadius = 0.3;
        float maxSphereRadius = 10.0;
        float traceDistance = (float)(10000);
        float3 rayOrigin = worldPos;
        float3 rayDir = lightUniformBlock.mLightDir;
        (rayOrigin += ((float3)(RayStartOffset) * rayDir));
        float3 rayEnd = (worldPos + (rayDir * (float3)(traceDistance)));
        float shadow = ShadowRayMarch(rayOrigin, rayEnd, (lightUniformBlock.mTanLightAngleAndThresholdValue).x, minSphereRadius, maxSphereRadius);
        OutTexture.write(float4(shadow, worldZ, 0.0, 0.0), DispatchThreadID.xy, 0);
    };

    Compute_Shader(
				   depth2d<float, access::sample> DepthTexture,texture3d<half> SDFVolumeTextureAtlas,texture2d<float, access::write> OutTexture,sampler clampToEdgeTrillinearSampler,sampler clampToEdgeNearSampler,constant Uniforms_cameraUniformBlock & cameraUniformBlock,constant Uniforms_meshSDFUniformBlock & meshSDFUniformBlock,constant Uniforms_lightUniformBlock & lightUniformBlock) :
DepthTexture(DepthTexture),SDFVolumeTextureAtlas(SDFVolumeTextureAtlas),OutTexture(OutTexture),clampToEdgeTrillinearSampler(clampToEdgeTrillinearSampler),clampToEdgeNearSampler(clampToEdgeNearSampler),cameraUniformBlock(cameraUniformBlock),meshSDFUniformBlock(meshSDFUniformBlock),lightUniformBlock(lightUniformBlock) {}
};

struct CSData {
    depth2d<float, access::sample> DepthTexture     [[id(0)]];
    texture3d<half> SDFVolumeTextureAtlas           [[id(1)]];
    texture2d<float, access::write> OutTexture      [[id(2)]];
    sampler clampToEdgeTrillinearSampler            [[id(3)]];
    sampler clampToEdgeNearSampler                  [[id(4)]];
};

struct CSDataPerFrame {
    constant Compute_Shader::Uniforms_cameraUniformBlock& cameraUniformBlock       [[id(0)]];
    constant Compute_Shader::Uniforms_meshSDFUniformBlock& meshSDFUniformBlock     [[id(1)]];
    constant Compute_Shader::Uniforms_lightUniformBlock& lightUniformBlock         [[id(2)]];
};

//[numthreads(16, 16, 1)]
kernel void stageMain(
    uint3 GroupID [[threadgroup_position_in_grid]],
    uint3 DispatchThreadID [[thread_position_in_grid]],
    uint3 GroupThreadID [[thread_position_in_threadgroup]],
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
    Compute_Shader main(csData.DepthTexture, csData.SDFVolumeTextureAtlas, csData.OutTexture, csData.clampToEdgeTrillinearSampler, csData.clampToEdgeNearSampler, csDataPerFrame.cameraUniformBlock, csDataPerFrame.meshSDFUniformBlock, csDataPerFrame.lightUniformBlock);
    return main.main(GroupID0, DispatchThreadID0, GroupThreadID0);
}
