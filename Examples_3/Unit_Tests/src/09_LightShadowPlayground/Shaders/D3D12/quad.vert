

cbuffer UniformQuadData : register(b0, UPDATE_FREQ_PER_FRAME)
{
	float4x4 mModelMat;
};

struct VSInput
{
	float4 Position : POSITION;
	float2 Tex_Coord : TEXCOORD0;
};

struct VSOutput
{
	float4 Position  : SV_POSITION;
	float2 Tex_Coord : TEXCOORD0;
};

/*-1.0f, 1.0f, 0.f,  0.0f, 0.0f,
	-1.0f, -1.0f, 0.f, 0.0f, 1.0f,
	1.0f, -1.0f, 0.f,  1.0f, 1.0f,

	1.0f, -1.0f, 0.f, 1.0f, 1.0f,
	1.0f, 1.0f, 0.f, 1.0f, 0.0f,
	-1.0f, 1.0f, 0.f, 0.0f, 0.0f,*/

VSOutput main(VSInput input, uint vertexID : SV_VertexID)
{
	float2 pos = float2(-1.0,1.0 );

	const uint verticesPerQuad = 6;
    uint quadID = vertexID / verticesPerQuad;
    uint quadVertexID = vertexID - quadID * verticesPerQuad;

    /*if( quadVertexID == 1 ) pos = float2(-1.0, -1.0 );
    if( quadVertexID == 2 ) pos = float2( 1.0,-1.0 );
    if( quadVertexID == 3 ) pos = float2( 1.0,-1.0 );
    if( quadVertexID == 4 ) pos = float2(1.0, 1.0 );
    if( quadVertexID == 5 ) pos = float2( -1.0, 1.0 );*/
	
	if( vertexID == 1 ) pos = float2(-1.0, -1.0 );
    if( vertexID == 2 ) pos = float2( 1.0,-1.0 );
    if( vertexID == 3 ) pos = float2( 1.0,-1.0 );
    if( vertexID == 4 ) pos = float2(1.0, 1.0 );
    if( vertexID == 5 ) pos = float2( -1.0, 1.0 );

	VSOutput result;
	result.Position = mul(mModelMat, float4(pos, 0.5, 1.0));
	//result.Position = float4(input.Position.xy, 0.0, 1.0);
	//result.Position.z = 0.8;


	result.Tex_Coord = input.Tex_Coord;

	return result;
}