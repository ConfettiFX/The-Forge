/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
 *
 * This is a part of Aura.
 * 
 * This file(code) is licensed under a 
 * Creative Commons Attribution-NonCommercial 4.0 International License 
 *
 *   (https://creativecommons.org/licenses/by-nc/4.0/legalcode) 
 *
 * Based on a work at https://github.com/ConfettiFX/The-Forge.
 * You may not use the material for commercial purposes.
 *
*/

#include "../../../Common_3/Utilities/Math/MathTypes.h"

#ifndef AxisAlignedBoundingBox_h
#define AxisAlignedBoundingBox_h

struct AxisAlignedBoundingBox 
{
    public:
    vec3 minPoint = vec3(INFINITY, INFINITY, INFINITY);
    vec3 maxPoint = -vec3(INFINITY, INFINITY, INFINITY);

    vec3 dimensions()
    {
        return maxPoint - minPoint;
    }
};

struct BoundingSphere 
{
    public:
        vec3 centre;
        float radius;

    BoundingSphere(AxisAlignedBoundingBox bb)
    {
        vec3 dimensions = bb.maxPoint - bb.minPoint;
        float longestDiagonal = sqrt(dimensions.getX()*dimensions.getX() + dimensions.getY()* dimensions.getY() + dimensions.getZ() * dimensions.getZ());

        this->centre = bb.minPoint + 0.5f * dimensions;
        this->radius = longestDiagonal * 0.5f;
    }
};

#endif // AxisAlignedBoundingBox_h