/*
 * Copyright (c) 2019 by Milos Tosic. All Rights Reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 *
 * MTuner SDK header
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE
 */

#ifndef RMEM_RMEM_H
#define RMEM_RMEM_H

#include <stdint.h>

/*--------------------------------------------------------------------------
 * Platforms
 *------------------------------------------------------------------------*/
#define RMEM_PLATFORM_WINDOWS		0
#define RMEM_PLATFORM_LINUX			0
#define RMEM_PLATFORM_IOS			0
#define RMEM_PLATFORM_OSX			0
#define RMEM_PLATFORM_PS3			0
#define RMEM_PLATFORM_PS4			0
#define RMEM_PLATFORM_ANDROID		0
#define RMEM_PLATFORM_XBOX360		0
#define RMEM_PLATFORM_XBOXONE		0
#define RMEM_PLATFORM_SWITCH        0

/*--------------------------------------------------------------------------
 * Compilers
 *------------------------------------------------------------------------*/
#define RMEM_COMPILER_MSVC			0
#define RMEM_COMPILER_GCC			0
#define RMEM_COMPILER_CLANG			0
#define RMEM_COMPILER_SNC			0

/*--------------------------------------------------------------------------
 * CPUs
 *------------------------------------------------------------------------*/
#define RMEM_CPU_X86				0
#define RMEM_CPU_PPC				0
#define RMEM_CPU_ARM				0
#define RMEM_CPU_MIPS				0

/*--------------------------------------------------------------------------
 * Endianess
 *------------------------------------------------------------------------*/
#define RMEM_LITTLE_ENDIAN			0
#define RMEM_BIG_ENDIAN				0

/*--------------------------------------------------------------------------
 * Word size
 *------------------------------------------------------------------------*/
#define RMEM_32BIT					0
#define RMEM_64BIT					0

/*--------------------------------------------------------------------------
 * Allocators
 *------------------------------------------------------------------------*/
#define RMEM_ALLOCATOR_DEFAULT		0
#define RMEM_ALLOCATOR_RPMALLOC		1
#define RMEM_ALLOCATOR_NOPROFILING	0x8000		/* added as OR mask to allocator IDto skip profiling */

/*--------------------------------------------------------------------------
 * Detect compiler
 *------------------------------------------------------------------------*/
#if defined(__SNC__)
#undef RMEM_COMPILER_SNC
#define RMEM_COMPILER_SNC			1

/* check for clang before GCC as clang defines GNU macros as well */
#elif defined(__clang__)
#undef RMEM_COMPILER_CLANG
#define RMEM_COMPILER_CLANG			1

#elif defined(__GNUC__)
#undef RMEM_COMPILER_GCC
#define RMEM_COMPILER_GCC			1

#elif defined(_MSC_VER)
#undef RMEM_COMPILER_MSVC
#define RMEM_COMPILER_MSVC			1

#else
#error "Compiler not supported!"
#endif

/*--------------------------------------------------------------------------
 * Detect platform
 *------------------------------------------------------------------------*/
#if defined(_XBOX_VER)
#undef  RMEM_PLATFORM_XBOX360
#define RMEM_PLATFORM_XBOX360		1
#elif defined(XBOX) || defined(_XBOX_ONE)
#undef  RMEM_PLATFORM_XBOXONE
#define RMEM_PLATFORM_XBOXONE		1
#elif defined(_WIN32) || defined(_WIN64) || defined(__WINDOWS__)
#undef RMEM_PLATFORM_WINDOWS
#define RMEM_PLATFORM_WINDOWS		1
#elif defined(__ANDROID__)
#undef RMEM_PLATFORM_ANDROID
#define RMEM_PLATFORM_ANDROID		1
#elif defined(__linux__) || defined(linux)
#undef RMEM_PLATFORM_LINUX
#define RMEM_PLATFORM_LINUX			1
#elif defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__)
#undef  RMEM_PLATFORM_IOS
#define RMEM_PLATFORM_IOS			1
#elif defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
#undef  RMEM_PLATFORM_OSX
#define RMEM_PLATFORM_OSX			1
#elif defined(__CELLOS_LV2__)
#undef RMEM_PLATFORM_PS3
#define RMEM_PLATFORM_PS3			1
#elif defined(__ORBIS__)
#undef RMEM_PLATFORM_PS4
#define RMEM_PLATFORM_PS4			1

#else
// CONFFX: Breaks Jenkins unnecessarily. 
// TODO: Uncomment when all platforms are supported.
// #error "Platform not supported!"
#endif

/*--------------------------------------------------------------------------
 * The Forge MTuner Integration Master Switch
 *------------------------------------------------------------------------*/
// Always off if platform not currently supported.
// List should contain all TF supported platforms in time. 
#if !(RMEM_PLATFORM_WINDOWS)

#ifdef USE_MTUNER
#undef USE_MTUNER
#endif

#define USE_MTUNER                  0

#endif

/*--------------------------------------------------------------------------
 * Detect CPU
 *------------------------------------------------------------------------*/
#if defined(__arm__) || (defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP))
#undef RMEM_CPU_ARM
#define RMEM_CPU_ARM				1
#define RMEM_CACHE_LINE_SIZE		64
#elif defined(__MIPSEL__) || defined(__mips_isa_rev)
#undef RMEM_CPU_MIPS
#define RMRM_CPU_MIPS				1
#define RMEM_CACHE_LINE_SIZE		64
#elif defined(_M_PPC) || defined(__powerpc__) || defined(__powerpc64__) || defined(__PPU__)
#undef RMEM_CPU_PPC
#define RMEM_CPU_PPC				1
#define RMEM_CACHE_LINE_SIZE		128
#elif defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
#undef RMEM_CPU_X86
#define RMEM_CPU_X86				1
#define RMEM_CACHE_LINE_SIZE		64
#else
// CONFFX: Breaks Jenkins unnecessarily. 
// TODO: Uncomment when all platforms are supported.
// #error "CPU not supported!"
#endif

/*--------------------------------------------------------------------------
 * Detect endianess
 *------------------------------------------------------------------------*/
#if RMEM_CPU_PPC
#undef RMEM_BIG_ENDIAN
#define RMEM_BIG_ENDIAN				1
#else
#undef RMEM_LITTLE_ENDIAN
#define RMEM_LITTLE_ENDIAN			1
#endif

/*--------------------------------------------------------------------------
 * 32bit or 64bit
 *------------------------------------------------------------------------*/
#if (defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) || defined(__amd64) || defined(__ppc64__) || defined(_WIN64) || defined(__LP64__) || defined(_LP64) )
#undef RMEM_64BIT
#define RMEM_64BIT					1
#else
#undef RMEM_32BIT
#define RMEM_32BIT					1
#endif

/*--------------------------------------------------------------------------
 * Log version macros
 *------------------------------------------------------------------------*/
#define RMEM_VER_HIGH				1
#define RMEM_VER_LOW				2

/*--------------------------------------------------------------------------
 * Memory marker is used to specify time points that are significant in
 * application lifetime in relation to memory allocation patterns.
 * An example would be a start of the main loop execution.
 *------------------------------------------------------------------------*/
#define MARKER_COLOR_DEFAULT		0xffffffff
#define MARKER_COLOR_RED			0xffff0000
#define MARKER_COLOR_GREEN			0xff00ff00
#define MARKER_COLOR_BLUE			0xff0000ff
#define MARKER_COLOR_YELLOW			0xffffff00
#define MARKER_COLOR_CYAN			0xff00ffff
#define MARKER_COLOR_PURPLE			0xffff00ff

/*--------------------------------------------------------------------------
 * Memory marker structure
 *------------------------------------------------------------------------*/
typedef struct
{
	const char*		m_name;			/* Marker name, always a string literal */
	uint32_t		m_nameHash;		/* Hashed marker name */
	uint32_t		m_color;		/* Marker color */

} RMemMarker;

/*--------------------------------------------------------------------------
 * Memory tag structure
 *------------------------------------------------------------------------*/
typedef struct
{
	const char*		m_name;			/* Tag name */
	uint32_t		m_HashedName;	/* Hashed tag name */

} RMemTag;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*--------------------------------------------------------------------------
 * API
 *------------------------------------------------------------------------*/

	/* Initialize memory library and set which type of hook to use */
	/* _data is used internally, pass 0 if manually instrumenting */
	void rmemInit(void* _data);

	/* If SDK was build with RMEM_ENABLE_DELAYED_CAPTURE defined to 1 (see rmem_config.h for details) */
	/* then no allocation tracking is done until stil function is called */
	void rmemStartCapture();

	/* CONFFX: Added to remove memory deallocation from MemoryHook destructor. */
	void rmemUnload(); 

	/* Shut down memory library and flush data */
	void rmemShutDown();

	/* Registers a memory tag with a name and name of its parent */
	void rmemRegisterTag(const char* _name, const char* _parentName);

	/* Pushes to stack a memory tag that is assigned to all following memory operations */
	void rmemEnterTag(RMemTag* _tag);

	/* Pops from stack a memory tag */
	void rmemLeaveTag(RMemTag* _tag);

	/* Registers memory marker with the tracking system */
	void rmemRegisterMarker(RMemMarker* _marker);

	/* Records an occurance of a memory marker */
	void rmemSetMarker(RMemMarker* _marker);

	/* Registers an allocator/heap handle with name */
	void rmemRegisterAllocator(const char* _name, uint64_t _handle);

	/* Called for each alloc operation */
	void rmemAlloc(uint64_t _handle, void* _ptr, uint32_t _size, uint32_t _overhead);

	/* Called for each realloc operation */
	void rmemRealloc(uint64_t _handle, void* _ptr, uint32_t _size, uint32_t _overhead, void* _prevPtr);

	/* Called for each aligned alloc operation */
	void rmemAllocAligned(uint64_t _handle, void* _ptr, uint32_t _size, uint32_t _overhead, uint32_t _alignment);

	/* Called for each aligned realloc operation */
	void rmemReallocAligned(uint64_t _handle, void* _ptr, uint32_t _size, uint32_t _overhead, void* _prevPtr, uint32_t _aAlignment);

	/* Called for each free operation */
	void rmemFree(uint64_t _handle, void* _ptr);

	/* Utility function to create a marker with predefined color */
	RMemMarker rmemCreateMarker(const char* _name, uint32_t _color);

	/* Utility function to create a marker with custom color */
	RMemMarker rmemCreateMarkerRGB(const char* _name, uint8_t _r, uint8_t _g, uint8_t _b);

	/* Utility function to create a memory tag with a specified name */
	RMemTag rmemCreateTag(const char* _name);

	/* Called on module load with name, base address and module size */
	void rmemAddModuleC(const char* _name, uint64_t _base, uint32_t _size);

	/* Called on module load with name, base address and module size */
	void rmemAddModuleW(const wchar_t* _name, uint64_t _base, uint32_t _size);
	
#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

/*--------------------------------------------------------------------------
 * Macros used to declare and activate a marker
 *------------------------------------------------------------------------*/

#define CONCAT2(_x, _y) _x ## _y
#define CONCAT(_x, _y) CONCAT2(_x, _y)

#define RMEM_MARKER(_name, _color)							\
	static RMemMarker	CONCAT(Marker,__LINE__) = 			\
	rmemCreateMarker(_name, _color);						\
	rmemSetMarker(&CONCAT(Marker,__LINE__));

#define RMEM_MARKER_RGB(_name, _r, _g, _b)					\
	static RMemMarker	CONCAT(Marker,__LINE__) =			\
	rmemCreateMarkerRGB(_name, _r, _g, _b);					\
	rmemSetMarker(&CONCAT(Marker,__LINE__));

#ifdef __cplusplus

/*--------------------------------------------------------------------------
 * Scoped activation of Memory tag
 *------------------------------------------------------------------------*/
struct RMemTagScope
{
	RMemTag*	m_tag;				// Tag for scoped tracking

	RMemTagScope(RMemTag* _tag);
	~RMemTagScope();
};

/*--------------------------------------------------------------------------
 * Utility structure used to register memory tags
 *------------------------------------------------------------------------*/
struct RMemTagRegistration
{
	RMemTagRegistration(const char* _name, const char* _parentName = 0);
};

/*--------------------------------------------------------------------------
 * Macros used to register memory tags
 *
 * Example usage:
 *		RMEM_REGISTER_TAG("Main loop")
 *		RMEM_REGISTER_TAG_CHILD("Update", "Main loop")
 *------------------------------------------------------------------------*/
#define RMEM_REGISTER_TAG(_name)									\
	RMemTagRegistration CONCAT(tag,__LINE__)(_name);				\

#define RMEM_REGISTER_TAG_CHILD(_name, _parentName)					\
	RMemTagRegistration CONCAT(tag,__LINE__)(_name, _parentName);	\

/*--------------------------------------------------------------------------
 * Macro used to activate memory tag on a scope basis
 *------------------------------------------------------------------------*/
#define RMEM_TAG(_name)												\
	static RMemTag CONCAT(tag,__LINE__) = rmemCreateTag(_name);		\
	RMemTagScope CONCAT(tagScope,__LINE__)(&CONCAT(tag,__LINE__));

#endif /* __cplusplus */

#endif /* RMEM_RMEM_H */

