// Renderer
#include "../IRay.h"

bool isRaytracingSupported(Renderer* /*pRenderer*/) {
	return false;
}

bool initRaytracing(Renderer* /*pRenderer*/, Raytracing** /*ppRaytracing*/) {
	return false;
}

void removeRaytracing(Renderer* /*pRenderer*/, Raytracing* /*pRaytracing*/) {}

void addAccelerationStructure(Raytracing* /*pRaytracing*/, const AccelerationStructureDescTop* /*pDesc*/, AccelerationStructure** /*ppAccelerationStructure*/) {}
void removeAccelerationStructure(Raytracing* /*pRaytracing*/, AccelerationStructure* /*pAccelerationStructure*/) {}

void addRaytracingShader(Raytracing* /*pRaytracing*/, const unsigned char* /*pByteCode*/, unsigned /*byteCodeSize*/, const char* /*pName*/, RaytracingShader** /*ppShader*/) {}
void removeRaytracingShader(Raytracing* /*pRaytracing*/, RaytracingShader* /*pShader*/) {}

void addRaytracingRootSignature(Raytracing* /*pRaytracing*/, const ShaderResource* /*pResources*/, uint32_t /*resourceCount*/, bool /*local*/, RootSignature** /*ppRootSignature*/, const RootSignatureDesc* /*pRootDesc */) {}

void addRaytracingPipeline(Raytracing* /*pRaytracing*/, const RaytracingPipelineDesc* /*pDesc*/, RaytracingPipeline** /*ppPipeline*/) {}
void removeRaytracingPipeline(Raytracing* /*pRaytracing*/, RaytracingPipeline* /*pPipeline*/) {}

void addRaytracingShaderTable(Raytracing* /*pRaytracing*/, const RaytracingShaderTableDesc* /*pDesc*/, RaytracingShaderTable** /*ppTable*/) {}
void removeRaytracingShaderTable(Raytracing* /*pRaytracing*/, RaytracingShaderTable* /*pTable*/) {}

void cmdBuildAccelerationStructure(Cmd* /*pCmd*/, Raytracing* /*pRaytracing*/, RaytracingBuildASDesc* /*pDesc*/) {}
void cmdDispatchRays(Cmd* /*pCmd*/, Raytracing* /*pRaytracing*/, const RaytracingDispatchDesc* /*pDesc*/) {}

void cmdCopyTexture(Cmd* /*pCmd*/, Texture* /*pDst*/, Texture* /*pSrc*/) {}
