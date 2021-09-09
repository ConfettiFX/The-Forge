/*******************************************************************************

Filename    :   System.cpp
Content     :	Global system functions.
Created     :   February 21, 2018
Authors     :   J.M.P. van Waveren, Jonathan Wright
Language    :   C++

Copyright:	Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*******************************************************************************/

#include "System.h"

#include <cstdio>
#include "time.h"
#include <string.h>

namespace OVRFW {

double GetTimeInSeconds() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec * 1e9 + now.tv_nsec) * 0.000000001;
}

} // namespace OVRFW
