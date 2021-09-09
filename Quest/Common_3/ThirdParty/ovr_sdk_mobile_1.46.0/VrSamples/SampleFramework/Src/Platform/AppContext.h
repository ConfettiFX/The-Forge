/*******************************************************************************

Filename	:   AppContext.h
Content		:   Opaque type representing application context
Created		:   March 20, 2018
Authors		:   Jonathan Wright
Language	:   C++

Copyright	:	Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*******************************************************************************/

#pragma once

#if defined(ANDROID)
#include "VrApi_Types.h"
#endif

namespace OVRFW {

// VrApi is currently Android-specific and passes ovrJava for what should
// ideally be opaque type holding OS-specific application context. This
// structure is currently a stand-in for that context, but is simply cast
// to ovrJava when vrapi_ API calls are made.
typedef struct {
} ovrContext;

class ovrAppContext {
   public:
    virtual ~ovrAppContext() {}

    virtual const ovrContext* ContextForVrApi() const = 0;

#if defined(ANDROID)
    operator const ovrJava*() {
        return reinterpret_cast<const ovrJava*>(ContextForVrApi());
    }
    operator const ovrJava&() {
        return *(reinterpret_cast<const ovrJava*>(ContextForVrApi()));
    }
#endif
};

} // namespace OVRFW
