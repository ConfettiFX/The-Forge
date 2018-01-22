struct _CBO
{
    float4 a;
    float4 b;
    float4 c;
    float4 d;
};

cbuffer CBO : register(c4)
{
    _CBO cbo[2][4];
};
struct _PushMe
{
    float4 a;
    float4 b;
    float4 c;
    float4 d;
};

cbuffer PushMe
{
    _PushMe push;
};

static float4 FragColor;

struct SPIRV_Cross_Output
{
    float4 FragColor : SV_Target0;
};

void frag_main()
{
    FragColor = cbo[1][2].a;
    FragColor += cbo[1][2].b;
    FragColor += cbo[1][2].c;
    FragColor += cbo[1][2].d;
    FragColor += push.a;
    FragColor += push.b;
    FragColor += push.c;
    FragColor += push.d;
}

SPIRV_Cross_Output main()
{
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
