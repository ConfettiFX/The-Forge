#define NODE_COUNT			(4)
#define RT_COUNT			(NODE_COUNT / 4)

struct ColorData
{
	uint4 color[RT_COUNT];
};

Texture2D<uint> Mask : register(t0, space0);
StructuredBuffer<ColorData> ColorDataSRV : register(t1, space0);

float4 UnpackRGBA(uint packedInput)
{
	float4 unpackedOutput;
	uint4 p = uint4((packedInput & 0xFFUL),
		(packedInput >> 8UL) & 0xFFUL,
		(packedInput >> 16UL) & 0xFFUL,
		(packedInput >> 24UL));

	unpackedOutput = ((float4)p) / 255.0;
	return unpackedOutput;
}

uint AddrGen(uint2 addr2D)
{
	uint2 dim;
	Mask.GetDimensions(dim[0], dim[1]);
	return addr2D[0] + dim[0] * addr2D[1];
}

void LoadData(in uint2 pixelAddr, out float4 nodeArray[NODE_COUNT])
{
	uint addr = AddrGen(pixelAddr);
	ColorData data = ColorDataSRV[addr];

	[unroll]for (uint i = 0; i < RT_COUNT; ++i)
	{
		[unroll]for (uint j = 0; j < 4; ++j)
		{
			float4 node = UnpackRGBA(data.color[i][j]);
			nodeArray[4 * i + j] = node;
		}
	}
}

struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 UV : Texcoord0;
};

float4 main(VSOutput input) : SV_Target
{
	uint2 pixelAddr = uint2(input.Position.xy);

	// Load all nodes for this pixel
	float4 nodeArray[NODE_COUNT];
	LoadData(pixelAddr, nodeArray);

	// Accumulate final transparent colors
	float a = 1.0f;
	float3 color = 0.0f;
	[unroll] for(uint i = 0; i < NODE_COUNT; ++i)
	{
		color += a * nodeArray[i].rgb;
		a = nodeArray[i].a;
	}

	return float4(color, nodeArray[NODE_COUNT - 1].a);
}
