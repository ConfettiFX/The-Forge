/* Write your header comments here */
#version 450 core


layout(location = 0) out vec2 vertOutput_TEXCOORD0;

struct PsIn
{
    vec4 Position;
    vec2 ScreenPos;
};
PsIn HLSLmain(uint vertexID)
{
    PsIn result;
    (((result).Position).x = float ((((vertexID == uint (2)))?(float (3.0)):(float ((-1.0))))));
    (((result).Position).y = float ((((vertexID == uint (0)))?(float ((-3.0))):(float (1.0)))));
    (((result).Position).zw = vec2(0, 1));
    ((result).ScreenPos = ((result).Position).xy);
    return result;
}
void main()
{
    uint vertexID;
    vertexID = gl_VertexIndex;
    PsIn result = HLSLmain(vertexID);
    gl_Position = result.Position;
    vertOutput_TEXCOORD0 = result.ScreenPos;
}
