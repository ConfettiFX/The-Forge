//
//  compute_resources.h
//

#ifndef compute_resources_h
#define compute_resources_h

// UPDATE_FREQ_NONE
RES(Tex2D(float),    depthBuffer,    UPDATE_FREQ_NONE, t1, binding = 1);
RES(Tex2D(float4),   normalBuffer,   UPDATE_FREQ_NONE, t2, binding = 2);
RES(Buffer(float4),  BVHTree,        UPDATE_FREQ_NONE, t3, binding = 3);
RES(Tex2D(float),    shadowbuffer,   UPDATE_FREQ_NONE, t4, binding = 4);
RES(Tex2D(float4),   albedobuffer,   UPDATE_FREQ_NONE, t5, binding = 8);
RES(Tex2D(float4),   lightbuffer,    UPDATE_FREQ_NONE, t6, binding = 6);
RES(WTex2D(float4), outputRT,       UPDATE_FREQ_NONE, u4, binding = 5);
RES(WTex2D(float),  outputShadowRT, UPDATE_FREQ_NONE, u9, binding = 9);

// UPDATE_FREQ_PER_FRAME
CBUFFER(cbPerPass, UPDATE_FREQ_PER_FRAME, b0, binding = 0)
{
	DATA(float4x4, projView, None);
	DATA(float4x4, invProjView, None);
	DATA(float4, rtSize, None);
	DATA(float4, lightDir, None);
	DATA(float4, cameraPos, None);
};

#endif /* compute_resources_h */
