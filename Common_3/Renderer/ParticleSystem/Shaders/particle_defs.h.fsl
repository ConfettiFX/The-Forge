/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef _PARTICLE_DEFS_H
#define _PARTICLE_DEFS_H

#define LIGHT_SIZE		0.4
#define SHADOW_PARTICLE_RADIUS	3

#define SHADOW_COUNT	8
#define LIGHT_COUNT		(10000 + SHADOW_COUNT)
#define STANDARD_COUNT	4002000
#define PARTICLE_COUNT	(STANDARD_COUNT + LIGHT_COUNT)

#define PARTICLES_BATCH_X		32
#define PARTICLES_BATCH_Y		1
#define MAX_PARTICLE_SET_COUNT	8
#define BOIDS_SAMPLE_COUNT		32

// ORBIS goes out of memory at 4K with 24 layers
#if defined(ORBIS)
#define MAX_TRANSPARENCY_LAYERS				16
#else
#define MAX_TRANSPARENCY_LAYERS				24
#endif

#define TRANSPARENCY_CONTRIBUTION_THRESHOLD	0.05

// The particle buffer is divided in 3 sections, one for each light mode
#define PARTICLE_BUFFER_SECTION_COUNT	3
// Each section is divided in 3 subsections: alive, dead, inactive
#define SUBSECTION_COUNT				3
// When swapping particles, we might receive a particle that has to be swapped as well. To reduce the 
// probability that a single thread gets to perform too many memory accesses, we try PARTICLE_SWAP_ATTEMPTS_COUNT
// times. Then we wait for the other threads to swap particles and finally we insert the particle.
#define PARTICLE_SWAP_ATTEMPTS_COUNT    2048
#define PARTICLE_SWAP_FAILURE_ATTEMPTS_COUNT	PARTICLE_SWAP_ATTEMPTS_COUNT
#define PARTICLE_SWAP_HELP_THRESHOLD			4
#define PARTICLE_SWAP_SELF_RESOLVE_THRESHOLD	8

// Maximum screen space area that allows for software rasterization. Bigger particles are forwarded to the hardware rasterizer
#define PARTICLE_HW_RASTERIZATION_THRESHOLD	2048
// Max particle set size, used to clamp position values between 0 and 1
#define PARTICLE_PACKING_SCALE	64.0
// Distance in units under which 2 particles are considered neighbors
#define	PARTICLE_NEIGHBOR_THRESHOLD	0.5


// Used to access previous/current values
#define PREV_VALUE	0
#define CURR_VALUE	1

#if defined(NO_FSL_DEFINITIONS) && !defined(CBUFFER)
    #define STATIC static
    #define STRUCT(NAME) struct NAME
    #define DATA(TYPE, NAME, SEM) TYPE NAME
    #define CBUFFER(NAME, FREQ, REG, BINDING) struct NAME
	#define float4x4 mat4
	#define RES(Type, Name, Frequency, u, b)
#endif

#define PARTICLE_BITFIELD_ALLOCATION_BITS_MASK 0x1FFFFU
#define PARTICLE_BITFIELD_SET_INDEX_MASK 0xFFFFU
#define PARTICLE_BITFIELD_IS_ALLOCATED 0x10000U

#define PARTICLE_BITFIELD_STATE_BITS_MASK 0xE0000U
#define PARTICLE_BITFIELD_IS_ALIVE 0x20000U
#define PARTICLE_BITFIELD_IS_MOVING 0x40000U
#define PARTICLE_BITFIELD_IS_ACCELERATING 0x80000U

#define PARTICLE_BITFIELD_TYPE_BITS_MASK		0x300000U
#define PARTICLE_BITFIELD_TYPE_RAIN				0x0U
#define PARTICLE_BITFIELD_TYPE_FIREFLIES		0x100000U
#define PARTICLE_BITFIELD_TYPE_FIREFLIES_BOIDS	0x200000U

#define PARTICLE_BITFIELD_COLLIDE_WITH_DEPTH_BUFFER 0x400000U

#define PARTICLE_BITFIELD_BILLBOARD_MODE_BITS_MASK 0x1800000U
#define PARTICLE_BITFIELD_BILLBOARD_MODE_SCREEN_ALIGNED 0x0U
#define PARTICLE_BITFIELD_BILLBOARD_MODE_VELOCITY_ORIENTED 0x800000U
#define PARTICLE_BITFIELD_BILLBOARD_MODE_HORIZONTAL 0x1000000U

#define PARTICLE_BITFIELD_LIGHTING_MODE_BITS_MASK		0x6000000U
#define PARTICLE_BITFIELD_LIGHTING_MODE_NONE			0x0U
#define PARTICLE_BITFIELD_LIGHTING_MODE_LIGHT			0x2000000U
#define PARTICLE_BITFIELD_LIGHTING_MODE_LIGHTNSHADOW	0x4000000U

#define PARTICLE_BITFIELD_LIGHT_CULLED					0x8000000U

STRUCT(ParticleData)
{
	DATA(uint2, VelocityAndAge, None);
	DATA(uint2, Position, None);
};

STRUCT(ParticleSet)
{
	// X,Y,Z: volume of the particle set, W: scale of its particles
	DATA(float4, Size, None);

	// Position of the particle set
	DATA(float3, Position, None);
	// Amount of particles to be spawned per second
	DATA(float, ParticlesPerSecond, None);

	// Color of all particles in the set
	DATA(float3, Color, None);
	DATA(float, LightRadius, None);

	// Type of particle, it establishes its behaviour (flocking, rain...)
	DATA(uint, ParticleType, None);
	// Light settings for the particle set, describes whether particles should cast shadows or emit lights
	DATA(uint, LightBitfield, None);
	// Maximum amount of particles to allocate
	DATA(uint, MaxParticles, None);
	// Lifetime of the particle
	DATA(float, InitialAge, None);

	// Forces for the flocking algorithm
	DATA(uint, BoidsAvoidSeekStrength, None);
	DATA(uint, BoidsSeparationFleeStrength, None);
	DATA(uint, BoidsCohesionAlignmentStrength, None);
	DATA(uint, SteeringStrengthMaxSpeed, None);
};

STRUCT(PackedParticleTransparencyNode)
{
	DATA(uint, Color, None);
	DATA(float, Depth, None);
};

STRUCT(ParticleTransparencyNode)
{
	// Color of the particle
	DATA(float4, Color, None);
	// Depth of the particle
	DATA(float, Depth, None);
};

CBUFFER(ParticleConstantBufferData, UPDATE_FREQ_PER_FRAME, b10, binding = 2)
{
	// Seed used to generate random values
	DATA(uint, Seed, None);
	// Total amount of particle sets in the particle system
	DATA(uint, ParticleSetCount, None);
	
	// Amount of dispatches in the Y direction
	DATA(uint, SimulationDispatchSize, None);
	// Reset particles (this is set to true when the app is loaded)
	DATA(uint, ResetParticles, None);
	// Time elapsed since the beginning of the app
	DATA(float, Time, None);
	// Delta time
	DATA(float, TimeDelta, None);
	
	// Screen size
	DATA(uint2, ScreenSize, None);
	// Point to follow when flocking
	DATA(float3, SeekPosition, None);
	
	DATA(float4x4, ViewTransform, None);
	DATA(float4x4, ProjTransform, None);
	DATA(float4x4, ViewProjTransform, None);
	DATA(float4, CameraPosition, None);
};

#endif
