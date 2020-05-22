#ifndef deferred_shader_pointlight_h
#define deferred_shader_pointlight_h

struct ArgData
{
#if SAMPLE_COUNT > 1
    texture2d_ms<float,access::read> gBufferColor;
    texture2d_ms<float,access::read> gBufferNormal;
    texture2d_ms<float,access::read> gBufferSpecular;
    texture2d_ms<float,access::read> gBufferSimulation;
    depth2d_ms<float,access::read> gBufferDepth;
#else
    texture2d<float,access::read> gBufferColor;
    texture2d<float,access::read> gBufferNormal;
    texture2d<float,access::read> gBufferSpecular;
    texture2d<float,access::read> gBufferSimulation ;
    depth2d<float,access::read> gBufferDepth;
#endif
	constant LightData* lights;
};

struct ArgDataPerFrame
{
    constant PerFrameConstants& uniforms;
};

#endif /* deferred_shader_pointlight_h */
