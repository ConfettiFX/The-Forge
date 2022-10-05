#pragma once

#ifndef __MT_MEMORY__
#define __MT_MEMORY__

#ifndef ORBIS
#include <alloca.h>
#endif

#define MT_ALLOCATE_ON_STACK(BYTES_COUNT) alloca(BYTES_COUNT)

#endif
