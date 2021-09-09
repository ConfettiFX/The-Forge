/************************************************************************************

Filename    :   ReflectionData.h
Content     :   Data for introspection and reflection of C++ objects.
Created     :   11/16/2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/
#pragma once

#include "Reflection.h"

#define MEMBER_SIZE(type_, member_) sizeof(((type_*)0)->member_)

namespace OVRFW {

//=============================================================================================
// Reflection Data
//=============================================================================================

extern ovrTypeInfo TypeInfoList[];

} // namespace OVRFW
