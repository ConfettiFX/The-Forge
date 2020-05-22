#pragma once

float2 OctWrap(float2 v)
{
	return (float2(1.0) - abs(v.yx)) * float2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0 );
}

float3 decodeDir(float2 encN)
{
	encN = encN * 2.0 - 1.0;

	float3 n;
	n.z = 1.0 - abs(encN.x) - abs(encN.y);
	n.xy = n.z >= 0.0 ? encN.xy : OctWrap(encN.xy);
	n = normalize(n);
	return n;
}

float2 unpack2Floats(uint p)
{
	return float2(as_type<half2>(p));
}
