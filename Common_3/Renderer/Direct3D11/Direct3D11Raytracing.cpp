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

void addRaytracingRootSignature(Raytracing* /*pRaytracing*/, const ShaderResource* /*pResources*/, uint32_t /*resourceCount*/, bool /*local*/, RootSignature** /*ppRootSignature*/, const RootSignatureDesc* /*pRootDesc */) {}

void addRaytracingShaderTable(Raytracing* /*pRaytracing*/, const RaytracingShaderTableDesc* /*pDesc*/, RaytracingShaderTable** /*ppTable*/) {}
void removeRaytracingShaderTable(Raytracing* /*pRaytracing*/, RaytracingShaderTable* /*pTable*/) {}

void cmdBuildAccelerationStructure(Cmd* /*pCmd*/, Raytracing* /*pRaytracing*/, RaytracingBuildASDesc* /*pDesc*/) {}
void cmdDispatchRays(Cmd* /*pCmd*/, Raytracing* /*pRaytracing*/, const RaytracingDispatchDesc* /*pDesc*/) {}

