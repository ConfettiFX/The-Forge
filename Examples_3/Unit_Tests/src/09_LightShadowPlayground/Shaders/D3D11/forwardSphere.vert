cbuffer objectUniformBlock
{
    row_major float4x4 WorldViewProjMat[26] : packoffset(c0);
    row_major float4x4 WorldMat[26] : packoffset(c104);
};

struct VsIn
{
    float4 PositionIn : POSITION;
    float4 NormalIn : NORMAL;
    uint InstanceIndex : SV_InstanceID;
};

struct PsIn
{
    float3 WorldPosition : POSITION;
    float3 Color : TEXCOORD1;
    float3 NormalOut : NORMAL;
    int IfPlane : TEXCOORD2;
    float4 Position : SV_Position;
};

PsIn main(VsIn input)
{
	PsIn output;
	output.Position = mul(input.PositionIn, WorldViewProjMat[input.InstanceIndex]);
    output.WorldPosition = mul(input.PositionIn, WorldMat[input.InstanceIndex]).xyz;
    if (input.InstanceIndex == 25)
    {
        output.NormalOut = float3(0.0f, 1.0f, 0.0f);
        output.IfPlane = 1;
    }
    else
    {
        output.NormalOut = mul(normalize(input.NormalIn.xyz), float3x3(WorldMat[input.InstanceIndex][0].xyz, WorldMat[input.InstanceIndex][1].xyz, WorldMat[input.InstanceIndex][2].xyz));
        output.IfPlane = 0;
    }
    output.Color = 1.0f.xxx;

    return output;
}
