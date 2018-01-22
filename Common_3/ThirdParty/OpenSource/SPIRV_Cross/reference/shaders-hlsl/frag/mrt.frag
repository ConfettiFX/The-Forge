static float4 RT0;
static float4 RT1;
static float4 RT2;
static float4 RT3;

struct SPIRV_Cross_Output
{
    float4 RT0 : SV_Target0;
    float4 RT1 : SV_Target1;
    float4 RT2 : SV_Target2;
    float4 RT3 : SV_Target3;
};

void frag_main()
{
    RT0 = float4(1.0f, 1.0f, 1.0f, 1.0f);
    RT1 = float4(2.0f, 2.0f, 2.0f, 2.0f);
    RT2 = float4(3.0f, 3.0f, 3.0f, 3.0f);
    RT3 = float4(4.0f, 4.0f, 4.0f, 4.0f);
}

SPIRV_Cross_Output main()
{
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.RT0 = RT0;
    stage_output.RT1 = RT1;
    stage_output.RT2 = RT2;
    stage_output.RT3 = RT3;
    return stage_output;
}
