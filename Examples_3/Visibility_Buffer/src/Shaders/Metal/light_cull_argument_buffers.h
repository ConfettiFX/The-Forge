#ifndef light_cull_argument_buffers_h
#define light_cull_argument_buffers_h

struct CSData
{
    constant LightData* lights [[id(0)]];
};

struct CSDataPerFrame
{
    device atomic_uint* lightClustersCount  [[id(0)]];
    device atomic_uint* lightClusters       [[id(1)]];
    constant PerFrameConstants& uniforms    [[id(2)]];
};

#endif /* light_cull_argument_buffers_h */
