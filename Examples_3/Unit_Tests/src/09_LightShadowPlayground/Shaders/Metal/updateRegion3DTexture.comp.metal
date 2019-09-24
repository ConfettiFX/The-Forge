/* Write your header comments here */
#include <metal_stdlib>
#include <metal_compute>
using namespace metal;

struct Compute_Shader
{
    constant float* SDFVolumeDataBuffer;
    texture3d<half, access::write> SDFVolumeTextureAtlas;
    struct Uniforms_UpdateSDFVolumeTextureAtlasCB
    {
        uint3 mSourceAtlasVolumeMinCoord;
        uint3 mSourceDimensionSize;
        uint3 mSourceAtlasVolumeMaxCoord;
    };
    constant Uniforms_UpdateSDFVolumeTextureAtlasCB & UpdateSDFVolumeTextureAtlasCB;
    void main(uint3 threadID, uint3 GTid)
    {
        if ((((((((UpdateSDFVolumeTextureAtlasCB.mSourceAtlasVolumeMinCoord).x > (threadID).x) || ((UpdateSDFVolumeTextureAtlasCB.mSourceAtlasVolumeMinCoord).y > (threadID).y)) || ((UpdateSDFVolumeTextureAtlasCB.mSourceAtlasVolumeMinCoord).z > (threadID).z)) || ((UpdateSDFVolumeTextureAtlasCB.mSourceAtlasVolumeMaxCoord).x < (threadID).x)) || ((UpdateSDFVolumeTextureAtlasCB.mSourceAtlasVolumeMaxCoord).y < (threadID).y)) || ((UpdateSDFVolumeTextureAtlasCB.mSourceAtlasVolumeMaxCoord).z < (threadID).z)))
        {
            return;
        }
        uint3 localThreadID = (threadID - UpdateSDFVolumeTextureAtlasCB.mSourceAtlasVolumeMinCoord);
        uint finalLocalIndex = (((((localThreadID).z * (UpdateSDFVolumeTextureAtlasCB.mSourceDimensionSize).x) * (UpdateSDFVolumeTextureAtlasCB.mSourceDimensionSize).y) + ((localThreadID).y * (UpdateSDFVolumeTextureAtlasCB.mSourceDimensionSize).x)) + (localThreadID).x);
        (SDFVolumeTextureAtlas.write(half4(SDFVolumeDataBuffer[finalLocalIndex], 0.0, 0.0, 0.0), uint3(threadID)));
		
    };

    Compute_Shader(
constant float* SDFVolumeDataBuffer,texture3d<half, access::write> SDFVolumeTextureAtlas,constant Uniforms_UpdateSDFVolumeTextureAtlasCB & UpdateSDFVolumeTextureAtlasCB) :
SDFVolumeDataBuffer(SDFVolumeDataBuffer),SDFVolumeTextureAtlas(SDFVolumeTextureAtlas),UpdateSDFVolumeTextureAtlasCB(UpdateSDFVolumeTextureAtlasCB) {}
};

struct CSData {
    texture3d<half, access::write> SDFVolumeTextureAtlas [[id(0)]];
};

struct CSDataPerFrame {
    constant float* SDFVolumeDataBuffer                                                             [[id(0)]];
    constant Compute_Shader::Uniforms_UpdateSDFVolumeTextureAtlasCB & UpdateSDFVolumeTextureAtlasCB [[id(1)]];
};

//[numthreads(8, 8, 8)]
kernel void stageMain(
                      uint3 threadID                            [[thread_position_in_grid]],
                      uint3 GTid                                [[thread_position_in_threadgroup]],
                      constant CSData& csData                   [[buffer(UPDATE_FREQ_NONE)]],
                      constant CSDataPerFrame& csDataPerFrame   [[buffer(UPDATE_FREQ_PER_FRAME)]]
)
{
    uint3 threadID0;
    threadID0 = threadID;
    uint3 GTid0;
    GTid0 = GTid;
    Compute_Shader main(csDataPerFrame.SDFVolumeDataBuffer, csData.SDFVolumeTextureAtlas, csDataPerFrame.UpdateSDFVolumeTextureAtlasCB);
    return main.main(threadID0, GTid0);
}
