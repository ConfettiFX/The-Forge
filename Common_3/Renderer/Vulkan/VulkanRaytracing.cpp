#ifdef VULKAN

#include "../IRay.h"

bool isRaytracingSupported(Renderer* /*pRenderer*/) {
	return false;
}

bool initRaytracing(Renderer* /*pRenderer*/, Raytracing** /*ppRaytracing*/) {
	return false;
}

void removeRaytracing(Renderer* /*pRenderer*/, Raytracing* /*pRaytracing*/) {
}

void cmdBuildAccelerationStructure(Cmd* /*pCmd*/, Raytracing* /*pRaytracing*/, RaytracingBuildASDesc* /*pDesc*/)
{
}

void addAccelerationStructure(Raytracing* /*pRaytracing*/, const AccelerationStructureDescTop* /*pDesc*/, AccelerationStructure** /*ppAccelerationStructure*/) {}
void removeAccelerationStructure(Raytracing* /*pRaytracing*/, AccelerationStructure* /*pAccelerationStructure*/) {}

extern void addRaytracingRootSignature(Renderer* pRenderer, const ShaderResource* pResources, uint32_t resourceCount,
	bool local, RootSignature** ppRootSignature, const RootSignatureDesc* pRootDesc = nullptr) {}

void addRaytracingPipeline(const RaytracingPipelineDesc* /*pDesc*/, Pipeline** /*ppPipeline*/) {}

void addRaytracingShaderTable(Raytracing* /*pRaytracing*/, const RaytracingShaderTableDesc* /*pDesc*/, RaytracingShaderTable** /*ppTable*/) {}
void removeRaytracingShaderTable(Raytracing* /*pRaytracing*/, RaytracingShaderTable* /*pTable*/) {}

void cmdDispatchRays(Cmd* /*pCmd*/, Raytracing* /*pRaytracing*/, const RaytracingDispatchDesc* /*pDesc*/) {}

#endif