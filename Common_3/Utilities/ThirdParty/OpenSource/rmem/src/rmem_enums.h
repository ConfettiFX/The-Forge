/*
 * Copyright (c) 2019 by Milos Tosic. All Rights Reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef RMEM_ENUMS_H
#define RMEM_ENUMS_H

namespace rmem {

//--------------------------------------------------------------------------
/// Defines stack trace storage per entry
//--------------------------------------------------------------------------
struct EntryTags
{
	enum Enum
	{
		Add		= 0xfe,
		Exists	= 0xff
	};
};

//--------------------------------------------------------------------------
/// Markers used to identify individual entries to the file
//--------------------------------------------------------------------------
struct LogMarkers
{
	enum Enum
	{
		OpAlloc,
		OpAllocAligned,
		OpCalloc,
		OpFree,
		OpRealloc,
		OpReallocAligned,

		OpCount,
		
		RegisterTag,
		EnterTag,
		LeaveTag,
		RegisterMarker,
		Marker,
		Module,
		Allocator
	};
};

//--------------------------------------------------------------------------
/// Toolchain/OS combination used to save the capture
//--------------------------------------------------------------------------
struct ToolChain
{
	enum Enum
	{
		Win_MSVC,
		Win_gcc,
		Linux_gcc,
		Linux_clang,
		OSX_gcc,
		OSX_clang,
		PS3_gcc,
		PS3_snc,
		PS4_clang,
		Android_arm,
		Android_mips,
		Android_x86,
		Xbox_360,
		Xbox_One,

		Custom1 = 100,
		Custom2,
		Custom3,
		Custom4,
		Custom5,
		Custom6,
		Custom7,
		Custom8,
		Custom9,
	
		Unknown = 0xff
	};
};

} // namespace rmem

#endif // RMEM_ENUMS_H
