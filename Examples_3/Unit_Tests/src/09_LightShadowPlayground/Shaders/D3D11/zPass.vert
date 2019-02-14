cbuffer objectUniformBlock : register(b0)
{
    row_major float4x4 WorldViewProjMat[26] : packoffset(c0);
};

struct VsIn
{
    float4 Position : POSITION;
    uint InstanceIndex : SV_InstanceID;
};

struct PsIn
{
    float4 Position : SV_Position;
};

PsIn main(VsIn input)
{
    PsIn output;
    output.Position = mul(input.Position, WorldViewProjMat[input.InstanceIndex]);
    return output;
}
