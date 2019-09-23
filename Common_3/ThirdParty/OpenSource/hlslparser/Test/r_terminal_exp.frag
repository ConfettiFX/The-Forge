struct Vertex_Output
{
	float4 Position : SV_POSITION;
	float2 UV   : TEXCOORD0;
};

Texture2D tex[20];
SamplerState smp[2];
 
float4 main( Vertex_Output In ): SV_Target
{
    return ((tex[2].Sample(smp[1], In.UV).xyzw).yzwx)[2];
}
