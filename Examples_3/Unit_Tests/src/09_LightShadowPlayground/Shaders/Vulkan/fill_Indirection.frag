/* Write your header comments here */
#version 450 core


layout(location = 0) in vec4 fragInput_TEXCOORD0;
layout(location = 0) out vec4 rast_FragData0; 

struct VSOutput
{
    vec4 Position;
    vec4 MiscData;
};
vec4 HLSLmain(VSOutput input1)
{
    return (input1).MiscData;
}
void main()
{
    VSOutput input1;
    input1.Position = vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w);
    input1.MiscData = fragInput_TEXCOORD0;
    vec4 result = HLSLmain(input1);
    rast_FragData0 = result;
}
