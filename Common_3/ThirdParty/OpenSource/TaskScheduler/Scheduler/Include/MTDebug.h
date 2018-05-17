// The MIT License (MIT)
//
// 	Copyright (c) 2015 Sergey Makeev, Vadim Slyusarev
//
// 	Permission is hereby granted, free of charge, to any person obtaining a copy
// 	of this software and associated documentation files (the "Software"), to deal
// 	in the Software without restriction, including without limitation the rights
// 	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// 	copies of the Software, and to permit persons to whom the Software is
// 	furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
// 	all copies or substantial portions of the Software.
//
// 	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// 	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// 	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// 	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// 	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// 	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// 	THE SOFTWARE.

#pragma once
#include "MTConfig.h"
#include "MTAppInterop.h"


#if defined(MT_DEBUG) || defined(MT_INSTRUMENTED_BUILD)

#define MT_REPORT_ASSERT_IMPL( condition, description, file, line ) MT::Diagnostic::ReportAssert(#condition, description, file, line);

#ifndef MT_REPORT_ASSERT
#define MT_REPORT_ASSERT( description ) { MT_REPORT_ASSERT_IMPL( "always", description, __FILE__, __LINE__ ) }
#endif

#ifndef MT_ASSERT
#define MT_ASSERT( condition, description ) { if ( !(condition) ) { MT_REPORT_ASSERT_IMPL( #condition, description, __FILE__, __LINE__ ) } }
#endif

#ifndef MT_VERIFY
#define MT_VERIFY( condition, description, operation ) { if ( !(condition) ) { { MT_REPORT_ASSERT_IMPL( #condition, description, __FILE__, __LINE__ ) }; operation; } }
#endif

#else

#ifndef MT_REPORT_ASSERT
#define MT_REPORT_ASSERT( description )
#endif

#ifndef MT_ASSERT
#define MT_ASSERT( condition, description )
#endif

#ifndef MT_VERIFY
#define MT_VERIFY( condition, description, operation ) { if ( !(condition) ) { operation; } }
#endif

#endif
