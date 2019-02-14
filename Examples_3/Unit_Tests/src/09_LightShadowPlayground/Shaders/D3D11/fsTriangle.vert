
struct VsIn
{
    uint VertexIndex : SV_VertexID;
};

struct PsIn
{
    float4 Position : SV_Position;
};

PsIn main(VsIn input)
{
	PsIn output;
    float x = (input.VertexIndex == 2) ? 3.0f : (-1.0f);
    float y = (input.VertexIndex == 0) ? (-3.0f) : 1.0f;
    output.Position = float4(x, y, 0.0f, 1.0f);
    return output;
}
