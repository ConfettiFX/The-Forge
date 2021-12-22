

#define THREAD_GROUP_SIZE 64

struct VertexIndices
{
	uint globalStrandIndex;
	uint localStrandIndex;
	uint globalVertexIndex;
	uint localVertexIndex;
	uint indexSharedMem;
};

struct StrandIndices
{
	uint globalStrandIndex;
	uint globalRootVertexIndex;
};

struct Capsule
{
	float4 center0AndRadius0;
	float4 center1AndRadius1;
};

// cbuffer cbSimulation : register(b0, UPDATE_FREQ_PER_DRAW)
CBUFFER(cbSimulation, UPDATE_FREQ_PER_DRAW, b0, binding = 0)
{
	DATA(float4x4, Transform, None);
	DATA(float4, QuatRotation, None);
#if HAIR_MAX_CAPSULE_COUNT > 0
	DATA(Capsule, Capsules[HAIR_MAX_CAPSULE_COUNT], None);
	DATA(uint, mCapsuleCount, None);
#endif
	DATA(float, Scale, None);
	DATA(uint, NumStrandsPerThreadGroup, None);
	DATA(uint, NumFollowHairsPerGuideHair, None);
	DATA(uint, NumVerticesPerStrand, None);
	DATA(float, Damping, None);
	DATA(float, GlobalConstraintStiffness, None);
	DATA(float, GlobalConstraintRange, None);
	DATA(float, VSPStrength, None);
	DATA(float, VSPAccelerationThreshold, None);
	DATA(float, LocalStiffness, None);
	DATA(uint, LocalConstraintIterations, None);
	DATA(uint, LengthConstraintIterations, None);
	DATA(float, TipSeperationFactor, None);
};
// cbuffer cbHairGlobal : register(b4, UPDATE_FREQ_PER_DRAW)
CBUFFER(cbHairGlobal, UPDATE_FREQ_PER_DRAW, b4, binding = 1)
{
	DATA(float4, Viewport, None);
	DATA(float4, Gravity, None);
	DATA(float4, Wind, None);
	DATA(float, TimeStep, None);
};

RES(RWBuffer(float4),HairVertexPositions, UPDATE_FREQ_PER_DRAW, u0, binding = 2);
RES(RWBuffer(float4),HairVertexPositionsPrev, UPDATE_FREQ_PER_DRAW, u1, binding = 3);
RES(RWBuffer(float4),HairVertexPositionsPrevPrev, UPDATE_FREQ_PER_DRAW, u2, binding = 4);
RES(RWBuffer(float4),HairVertexTangents, UPDATE_FREQ_PER_DRAW, u3, binding = 5);

RES(Buffer(float4), HairRestPositions, UPDATE_FREQ_PER_DRAW, t0, binding = 6);
RES(Buffer(float), HairRestLengths, UPDATE_FREQ_PER_DRAW, t1, binding = 7);
RES(Buffer(float4), HairGlobalRotations, UPDATE_FREQ_PER_DRAW, t2, binding = 8);
RES(Buffer(float4), HairRefsInLocalFrame, UPDATE_FREQ_PER_DRAW, t3, binding = 9);
RES(Buffer(float4), FollowHairRootOffsets, UPDATE_FREQ_PER_DRAW, t4, binding = 10);

DECLARE_RESOURCES()

float3 RotateVec(float4 q, float3 v)
{
	float3 uv, uuv;
	float3 qvec = float3(q.x, q.y, q.z);
	uv = cross(qvec, v);
	uuv = cross(qvec, uv);
	uv *= (2.0f * q.w);
	uuv *= 2.0f;

	return v + uv + uuv;
}

float4 InverseQuaternion(float4 q)
{
	float lengthSqr = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;

	if (lengthSqr < 0.001)
		return float4(0, 0, 0, 1.0f);

	q.x = -q.x / lengthSqr;
	q.y = -q.y / lengthSqr;
	q.z = -q.z / lengthSqr;
	q.w = q.w / lengthSqr;

	return q;
}

float4 NormalizeQuaternion(float4 q)
{
	float4 qq = q;
	float n = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;

	if (n < 1e-10f)
	{
		qq.w = 1;
		return qq;
	}

	qq *= 1.0f / sqrt(n);
	return qq;
}

float4 MultQuaternionAndQuaternion(float4 qA, float4 qB)
{
	float4 q;

	q.w = qA.w * qB.w - qA.x * qB.x - qA.y * qB.y - qA.z * qB.z;
	q.x = qA.w * qB.x + qA.x * qB.w + qA.y * qB.z - qA.z * qB.y;
	q.y = qA.w * qB.y + qA.y * qB.w + qA.z * qB.x - qA.x * qB.z;
	q.z = qA.w * qB.z + qA.z * qB.w + qA.x * qB.y - qA.y * qB.x;

	return q;
}

float4 MakeQuaternion(float angle_radian, float3 axis)
{
	// create quaternion using angle and rotation axis
	float4 quaternion;
	float halfAngle = 0.5f * angle_radian;
	float sinHalf = sin(halfAngle);

	quaternion.w = cos(halfAngle);
	quaternion.xyz = sinHalf * axis.xyz;

	return quaternion;
}

float4 QuatFromUnitVectors(float3 u, float3 v)
{
	float r = 1.f + dot(u, v);
	float3 n;

	// if u and v are parallel
	if (r < 1e-7)
	{
		r = 0.0f;
		n = abs(u.x) > abs(u.z) ? float3(-u.y, u.x, 0.f) : float3(0.f, -u.z, u.y);
	}
	else
	{
		n = cross(u, v);
	}

	float4 q = float4(n.x, n.y, n.z, r);
	return NormalizeQuaternion(q);
}


VertexIndices CalculateVertexIndices(uint localID, uint groupID)
{
	VertexIndices result;

	result.indexSharedMem = localID;

	result.localStrandIndex = localID % Get(NumStrandsPerThreadGroup);
	result.globalStrandIndex = groupID * Get(NumStrandsPerThreadGroup) + result.localStrandIndex;
	result.globalStrandIndex *= Get(NumFollowHairsPerGuideHair) + 1;
	result.localVertexIndex = (localID - result.localStrandIndex) / Get(NumStrandsPerThreadGroup);

	result.globalVertexIndex = result.globalStrandIndex * Get(NumVerticesPerStrand) + result.localVertexIndex;

	return result;
}

StrandIndices CalculateStrandIndices(uint localID, uint groupID)
{
	StrandIndices result;
	result.globalStrandIndex = THREAD_GROUP_SIZE * groupID + localID;
	result.globalStrandIndex *= Get(NumFollowHairsPerGuideHair) + 1;
	result.globalRootVertexIndex = result.globalStrandIndex * Get(NumVerticesPerStrand);
	return result;
}

float4 Integrate(float4 currentPosition, float4 prevPosition, float4 force, VertexIndices indices)
{
	float4 result = currentPosition;

	force.xyz += Get(Gravity).xyz;
	result.xyz += (1.0f - Get(Damping)) * (currentPosition.xyz - prevPosition.xyz) + force.xyz * Get(TimeStep) * Get(TimeStep);

	return result;
}

GroupShared(float4, sharedPos[THREAD_GROUP_SIZE]);
GroupShared(float4, sharedTangent[THREAD_GROUP_SIZE]);
GroupShared(float, sharedLength[THREAD_GROUP_SIZE]);