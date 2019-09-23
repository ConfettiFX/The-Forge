struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 UV : TEXCOORD0;
};

cbuffer RootConstant
{
	float stepSize;
};

Texture2D Source;
SamplerState LinearSampler;

float4 main(VSOutput input) : SV_Target
{    
	const int StepCount = 4;
	const float Weights[StepCount] = { 0.05092f, 0.44908f, 0.44908f, 0.05092f,  };
	const float Offsets[StepCount] = { -2.06278f, -0.53805f, 0.53805f, 2.06278f, };

	float4 output = 0.0f;
	[unroll] for (int i = 0; i < StepCount; ++i)
		output +=  Weights[i] * Source.Sample(LinearSampler, input.UV.xy + Offsets[i] * stepSize);

	return output;
}
