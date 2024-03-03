// ---------------------------------------------------------------------------------------------------------------------------------
//
//
//  _ __ ___  _ __ ___   __ _ _ __      ___ _ __  _ ___
// | '_ ` _ \| '_ ` _ \ / _` | '__|    / __| '_ \| '_  |
// | | | | | | | | | | | (_| | |    _ | (__| |_) | |_) |
// |_| |_| |_|_| |_| |_|\__, |_|   (_) \___| .__/| .__/
//                       __/ |             | |   | |
//                      |___/              |_|   |_|
//
// Memory manager & tracking software
//
// Best viewed with 8-character tabs and (at least) 132 columns
//
// ---------------------------------------------------------------------------------------------------------------------------------
//
// Restrictions & freedoms pertaining to usage and redistribution of this software:
//
//  * This software is 100% free
//  * If you use this software (in part or in whole) you must credit the author.
//  * This software may not be re-distributed (in part or in whole) in a modified
//    form without clear documentation on how to obtain a copy of the original work.
//  * You may not use this software to directly or indirectly cause harm to others.
//  * This software is provided as-is and without warrantee. Use at your own risk.
//
// For more information, visit HTTP://www.FluidStudios.com
//
// ---------------------------------------------------------------------------------------------------------------------------------
// Originally created on 12/22/2000 by Paul Nettle
//
// Copyright 2000, Fluid Studios, Inc., all rights reserved.
// ---------------------------------------------------------------------------------------------------------------------------------
//
// !!IMPORTANT!!
//
// This software is self-documented with periodic comments. Before you start using this software, perform a search for the string
// "-DOC-" to locate pertinent information about how to use this software.
//
// You are also encouraged to read the comment blocks throughout this source file. They will help you understand how this memory
// tracking software works, so you can better utilize it within your applications.
//
// NOTES:
//
// 1. If you get compiler errors having to do with set_new_handler, then go through this source and search/replace
//    "std::set_new_handler" with "set_new_handler".
//
// 2. This code purposely uses no external routines that allocate RAM (other than the raw allocation routines, such as malloc). We
//    do this because we want this to be as self-contained as possible. As an example, we don't use assert, because when running
//    under WIN32, the assert brings up a dialog box, which allocates RAM. Doing this in the middle of an allocation would be bad.
//
// 3. When trying to override new/delete under MFC (which has its own version of global new/delete) the linker will complain. In
//    order to fix this error, use the compiler option: /FORCE, which will force it to build an executable even with linker errors.
//    Be sure to check those errors each time you compile, otherwise, you may miss a valid linker error.
//
// 4. If you see something that looks odd to you or seems like a strange way of going about doing something, then consider that this
//    code was carefully thought out. If something looks odd, then just assume I've got a good reason for doing it that way (an
//    example is the use of the class MemStaticTimeTracker.)
//
// 5. With MFC applications, you will need to comment out any occurance of "#define new DEBUG_NEW" from all source files.
//
// 6. Include file dependencies are _very_important_ for getting the MMGR to integrate nicely into your application. Be careful if
//    you're including standard includes from within your own project includes; that will break this very specific dependency order.
//    It should look like this:
//
//		#include <stdio.h>   // Standard includes MUST come first
//		#include <stdlib.h>  //
//		#include <streamio>  //
//
//		#include "mmgr.h"    // mmgr.h MUST come next
//
//		#include "myfile1.h" // Project includes MUST come last
//		#include "myfile2.h" //
//		#include "myfile3.h" //
//
// ---------------------------------------------------------------------------------------------------------------------------------

// #include "stdafx.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../../../../OS/Interfaces/IOperatingSystem.h"
#include "../../../../Interfaces/ILog.h"

#if !defined(WIN32) && !defined(XBOX)
#include <unistd.h>
#endif

#include "mmgr.h"

#if MMGR_BACKTRACE
#if defined(_WIN32)
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#else
#include <execinfo.h>
#endif
#endif

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- If you're like me, it's hard to gain trust in foreign code. This memory manager will try to INDUCE your code to crash (for
// very good reasons... like making bugs obvious as early as possible.) Some people may be inclined to remove this memory tracking
// software if it causes crashes that didn't exist previously. In reality, these new crashes are the BEST reason for using this
// software!
//
// Whether this software causes your application to crash, or if it reports errors, you need to be able to TRUST this software. To
// this end, you are given some very simple debugging tools.
//
// The quickest way to locate problems is to enable the STRESS_TEST macro (below.) This should catch 95% of the crashes before they
// occur by validating every allocation each time this memory manager performs an allocation function. If that doesn't work, keep
// reading...
//
// If you enable the TEST_MEMORY_MANAGER #define (below), this memory manager will log an entry in the memory.log file each time it
// enters and exits one of its primary allocation handling routines. Each call that succeeds should place an "ENTER" and an "EXIT"
// into the log. If the program crashes within the memory manager, it will log an "ENTER", but not an "EXIT". The log will also
// report the name of the routine.
//
// Just because this memory manager crashes does not mean that there is a bug here! First, an application could inadvertantly damage
// the heap, causing malloc(), realloc() or free() to crash. Also, an application could inadvertantly damage some of the memory used
// by this memory tracking software, causing it to crash in much the same way that a damaged heap would affect the standard
// allocation routines.
//
// In the event of a crash within this code, the first thing you'll want to do is to locate the actual line of code that is
// crashing. You can do this by adding log() entries throughout the routine that crashes, repeating this process until you narrow
// in on the offending line of code. If the crash happens in a standard C allocation routine (i.e. malloc, realloc or free) don't
// bother contacting me, your application has damaged the heap. You can help find the culprit in your code by enabling the
// STRESS_TEST macro (below.)
//
// If you truely suspect a bug in this memory manager (and you had better be sure about it! :) you can contact me at
// midnight@FluidStudios.com. Before you do, however, check for a newer version at:
//
//	http://www.FluidStudios.com/publications.html
//
// When using this debugging aid, make sure that you are NOT setting the alwaysLogAll variable on, otherwise the log could be
// cluttered and hard to read.
// ---------------------------------------------------------------------------------------------------------------------------------

// #define	TEST_MEMORY_MANAGER

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- Enable this sucker if you really want to stress-test your app's memory usage, or to help find hard-to-find bugs
// ---------------------------------------------------------------------------------------------------------------------------------

// #define	STRESS_TEST

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- Enable this sucker if you want to stress-test your app's error-handling. Set RANDOM_FAIL to the percentage of failures you
//       want to test with (0 = none, >100 = all failures).
// ---------------------------------------------------------------------------------------------------------------------------------

// #define	RANDOM_FAILURE 10.0

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- Enable this sucker if you want to make sure you aren't getting lucky with memory alignment. If this is enabled and you ask
//       for memory aligned to 16 bytes (for example), you will only get memory aligned to 16 bytes, NOT 32+ bytes.
// ---------------------------------------------------------------------------------------------------------------------------------

// #define	FORCE_EXACT_ALIGNMENT

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- Locals -- modify these flags to suit your needs
// ---------------------------------------------------------------------------------------------------------------------------------

#ifdef STRESS_TEST
#define hashBits 12u
static bool               randomWipe = true;
static bool               alwaysValidateAll = true;
static bool               alwaysLogAll = true;
static bool               alwaysWipeAll = true;
static bool               cleanupLogOnFirstRun = true;
static const unsigned int paddingSize = 1024; // An extra 8K per allocation!
#else
#define hashBits 12u
static bool               randomWipe = false;
static bool               alwaysValidateAll = false;
static bool               alwaysLogAll = false;
static bool               alwaysWipeAll = true;
static bool               cleanupLogOnFirstRun = true;
static const unsigned int paddingSize = 4;
#endif

// ---------------------------------------------------------------------------------------------------------------------------------
// We define our own assert, because we don't want to bring up an assertion dialog, since that allocates RAM. Our new assert
// simply declares a forced breakpoint.
//
// The BEOS assert added by Arvid Norberg <arvid@iname.com>.
// ---------------------------------------------------------------------------------------------------------------------------------

#ifdef WIN32
#ifdef MEMORY_DEBUG
#define m_assert(x)   \
    if ((x) == false) \
    __debugbreak()
#else
#define m_assert(x)   \
    if ((x) == false) \
    __debugbreak()
#endif
#elif defined(__BEOS__)
#ifdef DEBUG
extern void               debugger(const char* message);
#define m_assert(x)   \
    if ((x) == false) \
    debugger("mmgr: assert failed")
#else
#define m_assert(x) \
    {               \
    }
#endif
#else // Linux uses assert, which we can use safely, since it doesn't bring up a dialog within the program.
#if defined(ORBIS) || defined(PROSPERO)
#ifdef MEMORY_DEBUG
#define m_assert(x) \
    if (!(x))       \
    __debugbreak()
#else
#define m_assert(x) \
    if (!(x))       \
    __debugbreak()
#endif
#else
#define m_assert(cond) assert(cond)
#endif
#define _unlink unlink
#if !defined(ORBIS) && !defined(PROSPERO)
#define localtime_s localtime_r
#endif
#define fopen_s(file, filename, mode) ((*file) = fopen(filename, mode))
#endif

#ifndef __has_feature
#define __has_feature(...) 0
#endif

#if defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)
#include <sanitizer/asan_interface.h>
#define ASAN_POISON(addr, size)   __asan_poison_memory_region(addr, size)
#define ASAN_UNPOISON(addr, size) __asan_unpoison_memory_region(addr, size)
#else
#define ASAN_POISON(...)
#define ASAN_UNPOISON(...)
#endif

// ---------------------------------------------------------------------------------------------------------------------------------
// Here, we turn off our macros because any place in this source file where the word 'new' or the word 'delete' (etc.)
// appear will be expanded by the macro. So to avoid problems using them within this source file, we'll just #undef them.
// ---------------------------------------------------------------------------------------------------------------------------------

#undef new
#undef delete
#undef malloc
#undef calloc
#undef realloc
#undef free

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- Get to know these values. They represent the values that will be used to fill unused and deallocated RAM.
// ---------------------------------------------------------------------------------------------------------------------------------

static unsigned int prefixPattern = 0xbaadf00d;   // Fill pattern for bytes preceeding allocated blocks
static unsigned int postfixPattern = 0xdeadc0de;  // Fill pattern for bytes following allocated blocks
static unsigned int unusedPattern = 0xfeedface;   // Fill pattern for freshly allocated blocks
static unsigned int releasedPattern = 0xdeadbeef; // Fill pattern for deallocated blocks

// ---------------------------------------------------------------------------------------------------------------------------------
// Other locals
// ---------------------------------------------------------------------------------------------------------------------------------

#define hashSize (1u << hashBits)
static const char*  allocationTypes[] = { "Unknown", "new", "new[]", "malloc", "calloc", "realloc", "delete", "delete[]", "free" };
static sAllocUnit*  hashTable[hashSize];
static sAllocUnit*  reservoir;
static unsigned int currentAllocationCount = 0;
static unsigned int breakOnAllocationCount = 0;
static sMStats      stats;
static const char*  sourceFile = "??";
static const char*  sourceFunc = "??";
static unsigned int sourceLine = 0;
static sAllocUnit** reservoirBuffer = NULL;
static unsigned int reservoirBufferSize = 0;
// static const char*  memoryLogFile = "memory.log";
static void         doCleanupLogOnFirstRun(void);
char*               LogToMemory(char* log);
const char*         mAppName;

#if MMGR_BACKTRACE
static int stackSkipCount = 0;
#ifdef _WIN32
// A unique handle to use for backtrace symbols, must be closed before exit
static HANDLE gProcessHandle;
#endif
#endif
//

// Mutex for different platforms

// Using critical section on windows instead of mutex to accelerate the lock/unlock process.
// Critical section is more like a lightweight mutex which could only shared within one process.
// For other platforms, the user should:
// 1. Define MUTEX to the corresponding mutex type
// 2. Define MUTEX_LOCK to call the lock function of the mutex. And need to create the mutex if it doesn't exists.
// 3. Define MUTEX_UNLOCK to call the unlock function of the mutex.
// 4. Add the mutex initialization function inside CreateMutex() on the bottom of this file.
// 5. Add the mutex destruction function inside RemoveMutex() on the bottom of this file. (Currently not used)

#include "../../../../Interfaces/IFileSystem.h"
#include "../../../../Interfaces/IThread.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))

typedef Mutex MUTEX;
// CreateMutex is a macro from windows.h from IOperatingSystem.h
#ifdef CreateMutex
#undef CreateMutex
#endif
#define MUTEX_LOCK(mutex)      \
    if (!mutex)                \
    {                          \
        mutex = CreateMutex(); \
    }                          \
    acquireMutex(mutex);
#define MUTEX_UNLOCK(mutex) releaseMutex(mutex);

MUTEX* allocMutex;
MUTEX* logMutex;

MUTEX* CreateMutex(void);
void   RemoveMutex(MUTEX** mutex);

// ---------------------------------------------------------------------------------------------------------------------------------
// Local functions only
// ---------------------------------------------------------------------------------------------------------------------------------

#define BUFFER_SIZE 2048

typedef struct tm tm;

inline static char* tf_strncpy(char* dst, size_t dstSize, const char* src, size_t count)
{
    size_t size = dstSize < count ? dstSize - 1 : count;
    char*  ret = strncpy(dst, src, size);
    ret[size] = '\0';
    return ret;
}
#define tf_strncpyarr(dst, src, count) tf_strncpy(dst, ARRAY_SIZE(dst), src, count)

inline static char* tf_strcpy(char* dst, size_t dstSize, const char* src)
{
    size_t srcSize = strlen(src);
    return tf_strncpy(dst, dstSize, src, srcSize);
}

#define tf_strcpyarr(dst, src)                tf_strcpy(dst, ARRAY_SIZE(dst), src)

#define tf_sprintf(dst, dstSize, format, ...) snprintf(dst, dstSize, format, __VA_ARGS__)

#define tf_sprintfarr(dst, format, ...)       tf_sprintf(dst, ARRAY_SIZE(dst), format, __VA_ARGS__)

static char* Log(const char* format, ...)
{
    // Cleanup the log?

    if (cleanupLogOnFirstRun)
        doCleanupLogOnFirstRun();

    // Build the buffer

    /*logMutex->lock();*/
    MUTEX_LOCK(logMutex);

    static char buffer[BUFFER_SIZE];
    va_list     ap;
    va_start(ap, format);
    const int charsWritten = vsnprintf(buffer, BUFFER_SIZE, format, ap);
    va_end(ap);

    // Open the log file

    // Too slow for writing to disk every time

    // FILE*fp = NULL;
    // fopen_s(&fp, memoryLogFile, "ab");

    //// If you hit this assert, then the memory logger is unable to log information to a file (can't open the file for some
    //// reason.) You can interrogate the variable 'buffer' to see what was supposed to be logged (but won't be.)
    // m_assert(fp);

    // if (!fp) return;

    //// Spit out the data to the log

    // fprintf(fp, "%s\r\n", buffer);
    // fclose(fp);

    // Add a newline to the end
    int       newlinePos = charsWritten;
    const int lastNewlinePos = BUFFER_SIZE - 2;
    newlinePos = newlinePos > lastNewlinePos ? lastNewlinePos : newlinePos;
    buffer[newlinePos] = '\n';
    buffer[newlinePos + 1] = '\0';

    // Quicker

    char* logAddress = LogToMemory(buffer);

    // logMutex->unlock();
    MUTEX_UNLOCK(logMutex);
    return logAddress;
}

// ---------------------------------------------------------------------------------------------------------------------------------

static void doCleanupLogOnFirstRun(void)
{
    if (cleanupLogOnFirstRun)
    {
        // #ifndef NX64
        //		_unlink(memoryLogFile);
        // #endif
        cleanupLogOnFirstRun = false;

        // Print a header for the log

        //		time_t t = time(NULL);
        //		tm     localt;
        // #ifdef _WIN32
        //		localtime_s(&localt, &t);
        // #else
        //        localtime_s(&t, &localt);
        // #endif
        // char asciiTime[64];
        // use strftime instead of asctime so we don't get the trailing newline. (We're writing the
        // strftime(asciiTime, 64, "%c", &localt);
        // Log("--------------------------------------------------------------------------------");
        // Log("");
        // Log("      %s - Memory logging file created on %s", memoryLogFile, asciiTime);
        // Log("");
        Log("--------------------------------------------------------------------------------");
        Log("");
        Log("This file contains a log of all memory operations performed during the last run.");
        Log("");
        Log("Interrogate this file to track errors or to help track down memory-related");
        Log("issues. You can do this by tracing the allocations performed by a specific owner");
        Log("or by tracking a specific address through a series of allocations and");
        Log("reallocations.");
        Log("");
        Log("There is a lot of useful information here which, when used creatively, can be");
        Log("extremely helpful.");
        Log("");
        Log("Note that the following guides are used throughout this file:");
        Log("");
        Log("   [!] - Error");
        Log("   [+] - Allocation");
        Log("   [~] - Reallocation");
        Log("   [-] - Deallocation");
        Log("   [I] - Generic information");
        Log("   [F] - Failure induced for the purpose of stress-testing your application");
        Log("   [D] - Information used for debugging this memory manager");
        Log("");
        Log("...so, to find all errors in the file, search for \"[!]\"");
        Log("");
        Log("--------------------------------------------------------------------------------");
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------

static const char* sourceFileStripper(const char* sourceFile)
{
    const char* ptr = strrchr(sourceFile, '\\');
    if (ptr)
        return ptr + 1;
    ptr = strrchr(sourceFile, '/');
    if (ptr)
        return ptr + 1;
    return sourceFile;
}

// ---------------------------------------------------------------------------------------------------------------------------------

static const char* ownerString(const char* sourceFile, const unsigned int sourceLine, const char* sourceFunc)
{
    static char str[180];
    memset(str, 0, sizeof(str));
    tf_sprintfarr(str, "%s(%05d)::%s", sourceFileStripper(sourceFile), sourceLine, sourceFunc);
    return str;
}

// ---------------------------------------------------------------------------------------------------------------------------------

static const char* insertCommas(unsigned int value)
{
    static char str[30];
    memset(str, 0, sizeof(str));

    tf_sprintfarr(str, "%u", value);
    if (strlen(str) > 3)
    {
        memmove(&str[strlen(str) - 3], &str[strlen(str) - 4], 4);
        str[strlen(str) - 4] = ',';
    }
    if (strlen(str) > 7)
    {
        memmove(&str[strlen(str) - 7], &str[strlen(str) - 8], 8);
        str[strlen(str) - 8] = ',';
    }
    if (strlen(str) > 11)
    {
        memmove(&str[strlen(str) - 11], &str[strlen(str) - 12], 12);
        str[strlen(str) - 12] = ',';
    }

    return str;
}

// ---------------------------------------------------------------------------------------------------------------------------------

static const char* memorySizeString(uint32_t size)
{
    static char str[90];
    if (size > (1024 * 1024))
        tf_sprintfarr(str, "%10s (%7.2fM)", insertCommas(size), ((float)size) / (1024.0f * 1024.0f));
    else if (size > 1024)
        tf_sprintfarr(str, "%10s (%7.2fK)", insertCommas(size), ((float)size) / 1024.0f);
    else
        tf_sprintfarr(str, "%10s bytes     ", insertCommas(size));
    return str;
}

// ---------------------------------------------------------------------------------------------------------------------------------

static sAllocUnit* findAllocUnit(const void* reportedAddress)
{
    // Just in case...
    m_assert(reportedAddress != NULL);

    // Use the address to locate the hash index. Note that we shift off the lower four bits. This is because most allocated
    // addresses will be on four-, eight- or even sixteen-byte boundaries. If we didn't do this, the hash index would not have
    // very good coverage.

    size_t      hashIndex = (((size_t)reportedAddress) >> 4) & (hashSize - 1);
    sAllocUnit* ptr = hashTable[hashIndex];
    while (ptr)
    {
        if (ptr->reportedAddress == reportedAddress)
            return ptr;
        ptr = ptr->next;
    }

    return NULL;
}

// ---------------------------------------------------------------------------------------------------------------------------------

static size_t calculateActualSize(const size_t reportedSize)
{
    // We use DWORDS as our padding, and a uint32_t is guaranteed to be 4 bytes, but an int is not (ANSI defines an int as
    // being the standard word size for a processor; on a 32-bit machine, that's 4 bytes, but on a 64-bit machine, it's
    // 8 bytes, which means an int can actually be larger than a uint32_t.)

    return reportedSize + paddingSize * sizeof(uint32_t) * 2;
}

// ---------------------------------------------------------------------------------------------------------------------------------

// static	size_t	calculateReportedSize(const size_t actualSize)
//{
//	// We use DWORDS as our padding, and a uint32_t is guaranteed to be 4 bytes, but an int is not (ANSI defines an int as
//	// being the standard word size for a processor; on a 32-bit machine, that's 4 bytes, but on a 64-bit machine, it's
//	// 8 bytes, which means an int can actually be larger than a uint32_t.)
//
//	return actualSize - paddingSize * sizeof(uint32_t) * 2;
// }

// ---------------------------------------------------------------------------------------------------------------------------------

static void* calculateReportedAddress(const void* actualAddress)
{
    // We allow this...

    if (!actualAddress)
        return NULL;

    // JUst account for the padding

    return (void*)(((const char*)actualAddress) + sizeof(uint32_t) * paddingSize);
}

// ---------------------------------------------------------------------------------------------------------------------------------

static void wipeWithPattern(sAllocUnit* allocUnit, uint32_t pattern, const unsigned int originalReportedSize)
{
    // For a serious test run, we use wipes of random a random value. However, if this causes a crash, we don't want it to
    // crash in a differnt place each time, so we specifically DO NOT call srand. If, by chance your program calls srand(),
    // you may wish to disable that when running with a random wipe test. This will make any crashes more consistent so they
    // can be tracked down easier.

    if (randomWipe)
    {
        pattern = ((rand() & 0xff) << 24) | ((rand() & 0xff) << 16) | ((rand() & 0xff) << 8) | (rand() & 0xff);
    }

    // -DOC- We should wipe with 0's if we're not in debug mode, so we can help hide bugs if possible when we release the
    // product. So uncomment the following line for releases.
    //
    // Note that the "alwaysWipeAll" should be turned on for this to have effect, otherwise it won't do much good. But we'll
    // leave it this way (as an option) because this does slow things down.
    //	pattern = 0;

    // This part of the operation is optional

    if (alwaysWipeAll && allocUnit->reportedSize > originalReportedSize)
    {
        // Fill the bulk

        uint32_t* lptr = (uint32_t*)(((char*)allocUnit->reportedAddress) + originalReportedSize);
        int       length = (int)(allocUnit->reportedSize - originalReportedSize);
        int       i;
        for (i = 0; i < (length >> 2); i++, lptr++)
        {
            *lptr = pattern;
        }

        // Fill the remainder

        unsigned int shiftCount = 0;
        char*        cptr = (char*)(lptr);
        for (i = 0; i < (length & 0x3); i++, cptr++, shiftCount += 8)
        {
            *cptr = (char)((pattern & (0xff << shiftCount)) >> shiftCount);
        }
    }

    // Write in the prefix/postfix bytes

    // Calculate the correct start addresses for pre and post patterns relative to
    // allocUnit->reportedAddress, since it may have been offset due to alignment requirements
    uint8_t* pre = (uint8_t*)allocUnit->reportedAddress - paddingSize * sizeof(uint32_t);
    uint8_t* post = (uint8_t*)allocUnit->reportedAddress + allocUnit->reportedSize;

    const size_t paddingBytes = paddingSize * sizeof(uint32_t);
    for (size_t i = 0; i < paddingBytes; i++, pre++, post++)
    {
        *pre = (prefixPattern >> ((i % sizeof(uint32_t)) * 8)) & 0xFF;
        *post = (postfixPattern >> ((i % sizeof(uint32_t)) * 8)) & 0xFF;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------
static void dumpLine(FileStream* fileToWrite, const char* format, ...)
{
    va_list args;
    char    buffer[BUFFER_SIZE] = { 0 };
    va_start(args, format);
    vsnprintf(buffer, BUFFER_SIZE, format, args);
    va_end(args);

    _OutputDebugString(buffer);
    _OutputDebugString("\n");
    if (fileToWrite != NULL && fileToWrite->pIO)
    {
        fsWriteToStream(fileToWrite, buffer, strlen(buffer));
        fsWriteToStream(fileToWrite, "\n", 1);
        fsFlushStream(fileToWrite);
    }
}

#if MMGR_BACKTRACE
static void dumpBacktrace(FileStream* fh, const sAllocUnit* ptr)
{
    if (ptr->backtrace_nptrs)
    {
#ifdef _WIN32
        HANDLE process = gProcessHandle;
        SymInitialize(process, NULL, TRUE);

        char         buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
        PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;

        char             buffer_line[sizeof(IMAGEHLP_LINE64)];
        PIMAGEHLP_LINE64 pLine = (PIMAGEHLP_LINE64)buffer_line;

        char module_name[FS_MAX_PATH];

        for (int i = ptr->backtrace_skip; i < ptr->backtrace_nptrs; i++)
        {
            DWORD64 address = (DWORD64)(ptr->backtrace_buffer[i]);

            // Get symbol name for address
            pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            pSymbol->MaxNameLen = MAX_SYM_NAME;
            SymFromAddr(process, address, 0, pSymbol);

            // Try to get line
            pLine->SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            DWORD displacement;
            if (SymGetLineFromAddr64(process, address, &displacement, pLine))
            {
                if (fh)
                {
                    dumpLine(fh, "    at %s in %s: line: %lu: address: 0x%0X", pSymbol->Name, pLine->FileName, pLine->LineNumber,
                             pSymbol->Address);
                }
                else
                {
                    Log("    at %s in %s: line: %lu: address: 0x%0X", pSymbol->Name, pLine->FileName, pLine->LineNumber, pSymbol->Address);
                }
            }
            else
            {
                // Try get the module name
                HMODULE hModule = NULL;
                module_name[0] = '\0';
                GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCTSTR)(address),
                                  &hModule);
                if (hModule != NULL)
                    GetModuleFileNameA(hModule, module_name, FS_MAX_PATH);

                if (fh)
                {
                    dumpLine(fh, "    at %s, address 0x%0X in %s", pSymbol->Name, pSymbol->Address, module_name);
                }
                else
                {
                    Log("    at %s, address 0x%0X in %s", pSymbol->Name, pSymbol->Address, module_name);
                }
            }
        }

        SymCleanup(process);
#else
        char** strings = backtrace_symbols(ptr->backtrace_buffer, ptr->backtrace_nptrs);
        if (strings != NULL)
        {
            for (int j = ptr->backtrace_skip; j < ptr->backtrace_nptrs; j++)
            {
                if (fh)
                {
                    dumpLine(fh, "\t%s", strings[j]);
                }
                else
                {
                    Log("\t%s", strings[j]);
                }
            }

            free(strings);
        }
#endif
    }
}
#endif

static void dumpAllocations(FileStream* fh)
{
    dumpLine(fh, "Alloc.        Addr           Size           Addr           Size                        BreakOn BreakOn");
    dumpLine(fh, "Number      Reported       Reported        Actual         Actual     Unused    Method  Dealloc Realloc  Allocated by");
    dumpLine(fh, "------ ------------------ ---------- ------------------ ---------- ---------- -------- ------- ------- "
                 "---------------------------------------------------");

    for (unsigned int i = 0; i < hashSize; i++)
    {
        sAllocUnit* ptr = hashTable[i];
        while (ptr)
        {
            dumpLine(fh, "% 6d 0x%016zX 0x%08zX 0x%016zX 0x%08zX 0x%08X %-8s    %c       %c    %s", ptr->allocationNumber,
                     (size_t)(ptr->reportedAddress), ptr->reportedSize, (size_t)(ptr->actualAddress), ptr->actualSize, mmgrCalcUnused(ptr),
                     allocationTypes[ptr->allocationType], ptr->breakOnDealloc ? 'Y' : 'N', ptr->breakOnRealloc ? 'Y' : 'N',
                     ownerString(ptr->sourceFile, ptr->sourceLine, ptr->sourceFunc));
#if MMGR_BACKTRACE
            dumpBacktrace(fh, ptr);
#endif
            ptr = ptr->next;
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------

static void dumpLeakReport(void)
{
    FileStream fh = { 0 };
    char       outputFilePath[256] = { 0 };

    if (mAppName)
    {
        // Open the report file
        // NOTE: we can't use any allocating FileSystem functions here since
        // the FileSystem may have already been destroyed by the time we get
        // here.
        char outputFileName[256] = { 0 };

        const char* extension = ".memleaks";

        tf_strcpyarr(outputFileName, mAppName);

        // Minimum length check
        if (outputFileName[0] == 0 || outputFileName[1] == 0)
        {
            tf_strcpyarr(outputFileName, "MemLeaks");
        }
        strcat(outputFileName, extension);

        const char* resourceDir = fsGetResourceDirectory(RD_LOG);
        tf_strcpyarr(outputFilePath, resourceDir);
        strcat(outputFilePath, "/");
        strcat(outputFilePath, outputFileName);

        if (!fsOpenStreamFromPath(RD_LOG, outputFileName, FM_WRITE, &fh))
        {
            memset(&fh, 0, sizeof fh);
            tf_strcpyarr(outputFilePath, "Failed to write report to file");
        }
    }

#if defined(__linux__) || defined(_WINDOWS) || defined(NX64)
    // #VK_NOTE: Vulkan driver leak workaround
    // When using validation layer + debug utils, some NVIDIA drivers are leaking memory which triggers leak report
    // since we provide our allocation callbacks (which use tf_malloc) to Vulkan driver
    // Ignore these driver leaks
    if (stats.totalAllocUnitCount)
    {
        for (unsigned int i = 0; i < hashSize; i++)
        {
            sAllocUnit* ptr = hashTable[i];
            while (ptr)
            {
                if (strstr(ptr->sourceFunc, "gVkAllocation") || strstr(ptr->sourceFunc, "gVkReallocation"))
                {
                    --stats.totalAllocUnitCount;
                }
                ptr = ptr->next;
            }
        }
    }
#endif

    {
        // Header
        time_t    t = time(NULL);
        struct tm tme;
#ifdef _WIN32
        localtime_s(&tme, &t);
#else
        localtime_s(&t, &tme);
#endif
        dumpLine(&fh, " -------------------------------------------------------------------------------");
        dumpLine(&fh, "                Memory leak report %02d/%02d/%04d %02d:%02d:%02d:                  ", tme.tm_mon + 1, tme.tm_mday,
                 tme.tm_year + 1900, tme.tm_hour, tme.tm_min, tme.tm_sec);
        dumpLine(&fh, "                %s                  ", outputFilePath);
        // use LF instead of CRLF
        dumpLine(&fh, " -------------------------------------------------------------------------------");
        if (stats.totalAllocUnitCount)
        {
            dumpLine(&fh, "%d memory leak%s found:\n", stats.totalAllocUnitCount, stats.totalAllocUnitCount == 1 ? "" : "s");
        }
        else
        {
            dumpLine(&fh, "Congratulations! No memory leaks found!");

            // We can finally free up our own memory allocations

            if (reservoirBuffer)
            {
                for (unsigned int i = 0; i < reservoirBufferSize; i++)
                {
                    free(reservoirBuffer[i]);
                }
                free(reservoirBuffer);
                reservoirBuffer = 0;
                reservoirBufferSize = 0;
                reservoir = NULL;
            }
        }

        if (stats.totalAllocUnitCount)
        {
            dumpAllocations(&fh);
        }

        char* allMemoryLog = Log("----All Allocations and Deallocations----");

        dumpLine(&fh, allMemoryLog);

        if (!stats.totalAllocUnitCount)
        {
            dumpLine(&fh, " ------------------------------------------------------------------------------");
            dumpLine(&fh, "Congratulations! No memory leaks found!");
            dumpLine(&fh, " ------------------------------------------------------------------------------");
        }
    }

    fsCloseStream(&fh);

    m_assert(stats.totalAllocUnitCount == 0 && "Memory leaks found");
}
// ---------------------------------------------------------------------------------------------------------------------------------
// We use a static class to let us know when we're in the midst of static deinitialization
// ---------------------------------------------------------------------------------------------------------------------------------
bool initMemAlloc(const char* appName)
{
    mAppName = appName;
    doCleanupLogOnFirstRun();

#if MMGR_BACKTRACE
#ifdef _WIN32
    // SymInitialize specifies that is should never be used with GetCurrentProcess and the handle should
    // be unique to avoid issues with sharing. A few libraries do it anyway though and it seems to work fine.
    // However avoid any undefined behaviour we create a real handle from GetCurrentProcess that we can
    // later use with SymInitialize.
    HANDLE currentProcess = GetCurrentProcess();
    DuplicateHandle(currentProcess, currentProcess, currentProcess, &gProcessHandle, 0, true, DUPLICATE_SAME_ACCESS);
#endif
#endif
    return true;
}

void exitMemAlloc(void)
{
    dumpLeakReport();

#if MMGR_BACKTRACE
#ifdef _WIN32
    CloseHandle(gProcessHandle);
#endif
#endif
}

MemoryStatistics memGetStatistics(void)
{
    MemoryStatistics result = {
        .totalReportedMemory = (uint32_t)stats.totalReportedMemory,
        .totalActualMemory = (uint32_t)stats.totalActualMemory,
        .peakReportedMemory = (uint32_t)stats.peakReportedMemory,
        .peakActualMemory = (uint32_t)stats.peakActualMemory,
        .accumulatedReportedMemory = (uint32_t)stats.accumulatedReportedMemory,
        .accumulatedActualMemory = (uint32_t)stats.accumulatedActualMemory,
        .accumulatedAllocUnitCount = (uint32_t)stats.accumulatedAllocUnitCount,
        .totalAllocUnitCount = (uint32_t)stats.totalAllocUnitCount,
        .peakAllocUnitCount = (uint32_t)stats.peakAllocUnitCount,
    };
    return result;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- Flags & options -- Call these routines to enable/disable the following options
// ---------------------------------------------------------------------------------------------------------------------------------

bool* m_alwaysValidateAll(void)
{
    // Force a validation of all allocation units each time we enter this software
    return &alwaysValidateAll;
}

// ---------------------------------------------------------------------------------------------------------------------------------

bool* m_alwaysLogAll(void)
{
    // Force a log of every allocation & deallocation into memory.log
    return &alwaysLogAll;
}

// ---------------------------------------------------------------------------------------------------------------------------------

bool* m_alwaysWipeAll(void)
{
    // Force this software to always wipe memory with a pattern when it is being allocated/dallocated
    return &alwaysWipeAll;
}

// ---------------------------------------------------------------------------------------------------------------------------------

bool* m_randomeWipe(void)
{
    // Force this software to use a random pattern when wiping memory -- good for stress testing
    return &randomWipe;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- Simply call this routine with the address of an allocated block of RAM, to cause it to force a breakpoint when it is
// reallocated.
// ---------------------------------------------------------------------------------------------------------------------------------

bool* mmgrBreakOnRealloc(void* reportedAddress)
{
    // Locate the existing allocation unit

    sAllocUnit* au = findAllocUnit(reportedAddress);

    // If you hit this assert, you tried to set a breakpoint on reallocation for an address that doesn't exist. Interrogate the
    // stack frame or the variable 'au' to see which allocation this is.
    m_assert(au != NULL);

    // If you hit this assert, you tried to set a breakpoint on reallocation for an address that wasn't allocated in a way that
    // is compatible with reallocation.
    m_assert(au->allocationType == m_alloc_malloc || au->allocationType == m_alloc_calloc || au->allocationType == m_alloc_realloc);

    return &au->breakOnRealloc;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- Simply call this routine with the address of an allocated block of RAM, to cause it to force a breakpoint when it is
// deallocated.
// ---------------------------------------------------------------------------------------------------------------------------------

bool* mmgrBreakOnDealloc(void* reportedAddress)
{
    // Locate the existing allocation unit

    sAllocUnit* au = findAllocUnit(reportedAddress);

    // If you hit this assert, you tried to set a breakpoint on deallocation for an address that doesn't exist. Interrogate the
    // stack frame or the variable 'au' to see which allocation this is.
    m_assert(au != NULL);

    return &au->breakOnDealloc;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- When tracking down a difficult bug, use this routine to force a breakpoint on a specific allocation count
// ---------------------------------------------------------------------------------------------------------------------------------

void m_breakOnAllocation(unsigned int count) { breakOnAllocationCount = count; }

// ---------------------------------------------------------------------------------------------------------------------------------
// Used by the macros
// ---------------------------------------------------------------------------------------------------------------------------------

void mmgrSetOwner(const char* file, const unsigned int line, const char* func)
{
    // You're probably wondering about this...
    //
    // It's important for this memory manager to primarily work with global new/delete in their original forms (i.e. with
    // no extra parameters.) In order to do this, we use macros that call this function prior to operators new & delete. This
    // is fine... usually. Here's what actually happens when you use this macro to delete an object:
    //
    // mmgrSetOwner(__FILE__, __LINE__, __FUNCTION__) --> object::~object() --> delete
    //
    // Note that the compiler inserts a call to the object's destructor just prior to calling our overridden operator delete.
    // But what happens when we delete an object whose destructor deletes another object, whose desctuctor deletes another
    // object? Here's a diagram (indentation follows stack depth):
    //
    // mmgrSetOwner(...) -> ~obj1()                          // original call to delete obj1
    //     mmgrSetOwner(...) -> ~obj2()                      // obj1's destructor deletes obj2
    //         mmgrSetOwner(...) -> ~obj3()                  // obj2's destructor deletes obj3
    //             ...                                     // obj3's destructor just does some stuff
    //         delete                                      // back in obj2's destructor, we call delete
    //     delete                                          // back in obj1's destructor, we call delete
    // delete                                              // back to our original call, we call delete
    //
    // Because mmgrSetOwner() just sets up some static variables (below) it's important that each call to mmgrSetOwner() and
    // successive calls to new/delete alternate. However, in this case, three calls to mmgrSetOwner() happen in succession
    // followed by three calls to delete in succession (with a few calls to destructors mixed in for fun.) This means that
    // only the final call to delete (in this chain of events) will have the proper reporting, and the first two in the chain
    // will not have ANY owner-reporting information. The deletes will still work fine, we just won't know who called us.
    //
    // "Then build a stack, my friend!" you might think... but it's a very common thing that people will be working with third-
    // party libraries (including MFC under Windows) which is not compiled with this memory manager's macros. In those cases,
    // mmgrSetOwner() is never called, and rightfully should not have the proper trace-back information. So if one of the
    // destructors in the chain ends up being a call to a delete from a non-mmgr-compiled library, the stack will get confused.
    //
    // I've been unable to find a solution to this problem, but at least we can detect it and report the data before we
    // lose it. That's what this is all about. It makes it somewhat confusing to read in the logs, but at least ALL the
    // information is present...
    //
    // There's a caveat here... The compiler is not required to call operator delete if the value being deleted is NULL.
    // In this case, any call to delete with a NULL will sill call mmgrSetOwner(), which will make mmgrSetOwner() think that
    // there is a destructor chain becuase we setup the variables, but nothing gets called to clear them. Because of this
    // we report a "Possible destructor chain".
    //
    // Thanks to J. Woznack (from Kodiak Interactive Software Studios -- www.kodiakgames.com) for pointing this out.

    if (sourceLine && alwaysLogAll)
    {
        Log("[I] NOTE! Possible destructor chain: previous owner is %s", ownerString(sourceFile, sourceLine, sourceFunc));
    }

    // Okay... save this stuff off so we can keep track of the caller

    sourceFile = file;
    sourceLine = line;
    sourceFunc = func;
}

void memSetStackSkipCount(int stackDepth)
{
#if MMGR_BACKTRACE
    // Only set if another call hasn't set this previously
    if (!stackSkipCount)
    {
        stackSkipCount = stackDepth;
    }
#endif
}

// ---------------------------------------------------------------------------------------------------------------------------------

static void resetGlobals(void)
{
    sourceFile = "??";
    sourceLine = 0;
    sourceFunc = "??";

#if MMGR_BACKTRACE
    stackSkipCount = 0;
#endif
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Allocate memory and track it
// ---------------------------------------------------------------------------------------------------------------------------------
void* mmgrAllocator(const char* sourceFile, const unsigned int sourceLine, const char* sourceFunc, const unsigned int allocationType,
                    size_t alignment, size_t reportedSize)
{
    if (cleanupLogOnFirstRun)
    {
        m_assert(false && "Memory tracker not initialized");
        return NULL;
    }

    // Make sure alignment is valid
    alignment = alignment < sizeof(void*) ? sizeof(void*) : alignment;

    // if (!allocMutex)
    //	allocMutex = CreateMutex();
    //

    MUTEX_LOCK(allocMutex);
    {
#ifdef TEST_MEMORY_MANAGER
        Log("[D] ENTER: mmgrAllocator()");
#endif

        // Increase our allocation count

        currentAllocationCount++;

        // Log the request

        if (alwaysLogAll)
            Log("[+] %05d %8s of size 0x%08X(%08d) by %s", currentAllocationCount, allocationTypes[allocationType], reportedSize,
                reportedSize, ownerString(sourceFile, sourceLine, sourceFunc));

        // If you hit this assert, you requested a breakpoint on a specific allocation count
        m_assert(currentAllocationCount != breakOnAllocationCount);

        // If necessary, grow the reservoir of unused allocation units

        if (!reservoir)
        {
            // Allocate 256 reservoir elements

            reservoir = (sAllocUnit*)malloc(sizeof(sAllocUnit) * 256);

            // If you hit this assert, then the memory manager failed to allocate internal memory for tracking the
            // allocations
            m_assert(reservoir != NULL);

            // Danger Will Robinson!

            if (reservoir == NULL)
            {
                printf("Unable to allocate RAM for internal memory tracking data");
                fflush(stdout);
                MUTEX_UNLOCK(allocMutex);
                m_assert(false && "Unable to allocate RAM for internal memory tracking data");
            }
            // Build a linked-list of the elements in our reservoir

            memset(reservoir, 0, sizeof(sAllocUnit) * 256);
            for (unsigned int i = 0; i < 256 - 1; i++)
            {
                reservoir[i].next = &reservoir[i + 1];
            }

            // Add this address to our reservoirBuffer so we can free it later

            sAllocUnit** temp = (sAllocUnit**)realloc(reservoirBuffer, (reservoirBufferSize + 1) * sizeof(sAllocUnit*));
            m_assert(temp);
            if (temp)
            {
                reservoirBuffer = temp;
                reservoirBuffer[reservoirBufferSize++] = reservoir;
            }
        }

        // Logical flow says this should never happen...
        m_assert(reservoir != NULL);

        // Grab a new allocaton unit from the front of the reservoir

        sAllocUnit* au = reservoir;
        reservoir = au->next;

        // Populate it with some real data

        memset(au, 0, sizeof(sAllocUnit));
        au->actualSize = calculateActualSize(reportedSize) + alignment;
#ifdef FORCE_EXACT_ALIGNMENT
        au->actualSize += alignment;
#endif
#ifdef RANDOM_FAILURE
        double a = rand();
        double b = RAND_MAX / 100.0 * RANDOM_FAILURE;
        if (a > b)
        {
            au->actualAddress = malloc(au->actualSize);
        }
        else
        {
            Log("[F] Random faiure");
            au->actualAddress = NULL;
        }
#else
        au->actualAddress = malloc(au->actualSize);
#endif
        au->reportedSize = reportedSize;
        au->reportedAddress = calculateReportedAddress(au->actualAddress);
        au->alignment = alignment;
        au->allocationType = allocationType;
        au->sourceLine = sourceLine;
        au->allocationNumber = currentAllocationCount;

        // Make sure the address we return to user is aligned to the specified alignment
        size_t offset = ((size_t)au->reportedAddress) % alignment;
        if (offset)
        {
            au->reportedAddress = (uint8_t*)au->reportedAddress + (alignment - offset);
        }

#ifdef FORCE_EXACT_ALIGNMENT
        if (!((size_t)au->reportedAddress & alignment))
        {
            // Try to "unalign" this pointer as much as possible, to make repeatable the
            // "random" failures during testing due to insufficiently strict alignment
            // during allocations
            au->reportedAddress = (uint8_t*)au->reportedAddress + alignment;
            offset += alignment;
        }
#endif

        au->offset = offset;

        if (sourceFile)
            tf_strncpyarr(au->sourceFile, sourceFileStripper(sourceFile), sizeof(au->sourceFile) - 1);
        else
            tf_strcpyarr(au->sourceFile, "??");
        if (sourceFunc)
            tf_strncpyarr(au->sourceFunc, sourceFunc, sizeof(au->sourceFunc) - 1);
        else
            tf_strcpyarr(au->sourceFunc, "??");

#if MMGR_BACKTRACE
#ifdef _WIN32
        au->backtrace_nptrs = CaptureStackBackTrace(stackSkipCount + 1, MMGR_BACKTRACE_SIZE, au->backtrace_buffer, NULL);
        // Skipped for us above
        au->backtrace_skip = 0;
#else
        au->backtrace_nptrs = backtrace(au->backtrace_buffer, MMGR_BACKTRACE_SIZE);
        au->backtrace_skip = stackSkipCount + 1;
#endif
#endif

        // We don't want to assert with random failures, because we want the application to deal with them.

#ifndef RANDOM_FAILURE
        // If you hit this assert, then the requested allocation simply failed (you're out of memory.) Interrogate the
        // variable 'au' or the stack frame to see what you were trying to do.
        m_assert(au->actualAddress != NULL);
#endif

        if (au->actualAddress == NULL)
        {
            printf("Request for allocation failed. Out of memory.");
            fflush(stdout);
            MUTEX_UNLOCK(allocMutex);
            m_assert(false && "Request for allocation failed. Out of memory.");
        }

        // If you hit this assert, then this allocation was made from a source that isn't setup to use this memory tracking
        // software, use the stack frame to locate the source and include our H file.
        m_assert(allocationType != m_alloc_unknown);

        // Insert the new allocation into the hash table

        size_t hashIndex = (((size_t)au->reportedAddress) >> 4) & (hashSize - 1);
        if (hashTable[hashIndex])
            hashTable[hashIndex]->prev = au;
        au->next = hashTable[hashIndex];
        au->prev = NULL;
        hashTable[hashIndex] = au;

        // Account for the new allocatin unit in our stats

        stats.totalReportedMemory += (unsigned int)(au->reportedSize);
        stats.totalActualMemory += (unsigned int)(au->actualSize);
        stats.totalAllocUnitCount++;
        if (stats.totalReportedMemory > stats.peakReportedMemory)
            stats.peakReportedMemory = stats.totalReportedMemory;
        if (stats.totalActualMemory > stats.peakActualMemory)
            stats.peakActualMemory = stats.totalActualMemory;
        if (stats.totalAllocUnitCount > stats.peakAllocUnitCount)
            stats.peakAllocUnitCount = stats.totalAllocUnitCount;
        stats.accumulatedReportedMemory += (unsigned int)(au->reportedSize);
        stats.accumulatedActualMemory += (unsigned int)(au->actualSize);
        stats.accumulatedAllocUnitCount++;

        // Prepare the allocation unit for use (wipe it with recognizable garbage)

        wipeWithPattern(au, unusedPattern, 0);

        // calloc() expects the reported memory address range to be filled with 0's

        if (allocationType == m_alloc_calloc)
        {
            memset(au->reportedAddress, 0, au->reportedSize);
        }

        // Validate every single allocated unit in memory

        if (alwaysValidateAll)
            mmgrValidateAllAllocUnits();

        // Log the result

        if (alwaysLogAll)
            Log("[+] ---->             addr 0x%08zX", (size_t)(au->reportedAddress));

        // Resetting the globals insures that if at some later time, somebody calls our memory manager from an unknown
        // source (i.e. they didn't include our H file) then we won't think it was the last allocation.

        resetGlobals();

        // Return the (reported) address of the new allocation unit

#ifdef TEST_MEMORY_MANAGER
        Log("[D] EXIT : mmgrAllocator()");
#endif

        MUTEX_UNLOCK(allocMutex);
        return au->reportedAddress;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Reallocate memory and track it
// ---------------------------------------------------------------------------------------------------------------------------------

void* mmgrReallocator(const char* sourceFile, const unsigned int sourceLine, const char* sourceFunc, const unsigned int reallocationType,
                      size_t reportedSize, void* reportedAddress)
{
    // Round up the size to a multiple of sizeof(uint32_t) so we don't write to misaligned pointers in wipeWithPattern.
    reportedSize += sizeof(uint32_t) - 1;
    reportedSize &= ~(sizeof(uint32_t) - 1);

    /*if (!allocMutex)
    allocMutex = CreateMutex();
    allocMutex->lock();*/
    MUTEX_LOCK(allocMutex);

    {
#ifdef TEST_MEMORY_MANAGER
        Log("[D] ENTER: mmgrReallocator()");
#endif

        // Calling realloc with a NULL should force same operations as a malloc

        if (!reportedAddress)
        {
            MUTEX_UNLOCK(allocMutex);
            return mmgrAllocator(sourceFile, sourceLine, sourceFunc, reallocationType, sizeof(void*), reportedSize);
        }

        // Increase our allocation count

        currentAllocationCount++;

        // If you hit this assert, you requested a breakpoint on a specific allocation count
        m_assert(currentAllocationCount != breakOnAllocationCount);

        // Log the request

        if (alwaysLogAll)
            Log("[~] %05d %8s of size 0x%08X(%08d) by %s", currentAllocationCount, allocationTypes[reallocationType], reportedSize,
                reportedSize, ownerString(sourceFile, sourceLine, sourceFunc));

        // Locate the existing allocation unit

        sAllocUnit*  au = findAllocUnit(reportedAddress);
        const size_t alignment = au->alignment;
        const size_t oldReportedSize = au->reportedSize;

        // If you hit this assert, you tried to reallocate RAM that wasn't allocated by this memory manager.
        m_assert(au != NULL);
        if (au == NULL)
        {
            printf("Request to reallocate RAM that was never allocated");
            fflush(stdout);
            MUTEX_UNLOCK(allocMutex);
            m_assert(false && "Request to reallocate RAM that was never allocated");
        }
        // If you hit this assert, then the allocation unit that is about to be reallocated is damaged. But you probably
        // already know that from a previous assert you should have seen in validateAllocUnit() :)
        m_assert(mmgrValidateAllocUnit(au));

        // If you hit this assert, then this reallocation was made from a source that isn't setup to use this memory
        // tracking software, use the stack frame to locate the source and include our H file.
        m_assert(reallocationType != m_alloc_unknown);

        // If you hit this assert, you were trying to reallocate RAM that was not allocated in a way that is compatible with
        // realloc. In other words, you have a allocation/reallocation mismatch.
        m_assert(au->allocationType == m_alloc_malloc || au->allocationType == m_alloc_calloc || au->allocationType == m_alloc_realloc);

        // If you hit this assert, then the "break on realloc" flag for this allocation unit is set (and will continue to be
        // set until you specifically shut it off. Interrogate the 'au' variable to determine information about this
        // allocation unit.
        m_assert(au->breakOnRealloc == false);

        // Keep track of the original size

        unsigned int originalReportedSize = (unsigned int)(au->reportedSize);

        if (alwaysLogAll)
            Log("[~] ---->             from 0x%08X(%08d)", originalReportedSize, originalReportedSize);

        // Do the reallocation

        void*  oldReportedAddress = reportedAddress;
        size_t newActualSize = calculateActualSize(reportedSize) + alignment;
#ifdef FORCE_EXACT_ALIGNMENT
        newActualSize += alignment;
#endif
        void* newActualAddress = NULL;

        // We need copy of old data in case the address we get from realloc has different alignment
        // This would mean reportedAddress points to a different offset in memory
        // Another solution is using memmove
        const size_t minReportedSize = oldReportedSize < reportedSize ? oldReportedSize : reportedSize;
        void*        oldData = malloc(minReportedSize);
        memcpy(oldData, oldReportedAddress, minReportedSize);

#ifdef RANDOM_FAILURE
        double a = rand();
        double b = RAND_MAX / 100.0 * RANDOM_FAILURE;
        if (a > b)
        {
            newActualAddress = realloc(au->actualAddress, newActualSize);
        }
        else
        {
            Log("[F] Random faiure");
        }
#else
        newActualAddress = realloc(au->actualAddress, newActualSize);
#endif

        // We don't want to assert with random failures, because we want the application to deal with them.

#ifndef RANDOM_FAILURE
        // If you hit this assert, then the requested allocation simply failed (you're out of memory) Interrogate the
        // variable 'au' to see the original allocation. You can also query 'newActualSize' to see the amount of memory
        // trying to be allocated. Finally, you can query 'reportedSize' to see how much memory was requested by the caller.
        m_assert(newActualAddress);
#endif

        if (!newActualAddress)
        {
            printf("Request for reallocation failed. Out of memory.");
            fflush(stdout);
            MUTEX_UNLOCK(allocMutex);
            m_assert(false && "Request for reallocation failed. Out of memory.");
        }
        // Remove this allocation from our stats (we'll add the new reallocation again later)

        stats.totalReportedMemory -= (unsigned int)(au->reportedSize);
        stats.totalActualMemory -= (unsigned int)(au->actualSize);

        // Update the allocation with the new information

        au->actualSize = newActualSize;
        au->actualAddress = newActualAddress;
        au->reportedSize = reportedSize;
        au->reportedAddress = calculateReportedAddress(newActualAddress);
        au->allocationType = reallocationType;
        au->sourceLine = sourceLine;
        au->allocationNumber = currentAllocationCount;

        // Make sure the address we return to user is aligned to the specified alignment
        size_t offset = ((size_t)au->reportedAddress) % alignment;
        if (offset)
        {
            au->reportedAddress = (uint8_t*)au->reportedAddress + (alignment - offset);
        }

#ifdef FORCE_EXACT_ALIGNMENT
        if (!((size_t)au->reportedAddress & alignment))
        {
            // Try to "unalign" this pointer as much as possible, to make repeatable the
            // "random" failures during testing due to insufficiently strict alignment
            // during allocations
            au->reportedAddress = (uint8_t*)au->reportedAddress + alignment;
            offset += alignment;
        }
#endif

        // Case where the new address has different alignment in which case we need to copy the old data as to respect realloc guarantees
        if (offset != au->offset)
        {
            // Copy old data
            size_t minReportedSize = oldReportedSize < reportedSize ? oldReportedSize : reportedSize;
            memcpy(au->reportedAddress, oldData, minReportedSize);
            au->offset = offset;
        }

        free(oldData);

        if (sourceFile)
            tf_strncpyarr(au->sourceFile, sourceFileStripper(sourceFile), sizeof(au->sourceFile) - 1);
        else
            tf_strcpyarr(au->sourceFile, "??");
        if (sourceFunc)
            tf_strncpyarr(au->sourceFunc, sourceFunc, sizeof(au->sourceFunc) - 1);
        else
            tf_strcpyarr(au->sourceFunc, "??");

#if MMGR_BACKTRACE
#ifdef _WIN32
        au->backtrace_nptrs = CaptureStackBackTrace(stackSkipCount + 1, MMGR_BACKTRACE_SIZE, au->backtrace_buffer, NULL);
        // Skipped for us above
        au->backtrace_skip = 0;
#else
        au->backtrace_nptrs = backtrace(au->backtrace_buffer, MMGR_BACKTRACE_SIZE);
        au->backtrace_skip = stackSkipCount + 1;
#endif
#endif

        // The reallocation may cause the address to change, so we should relocate our allocation unit within the hash table

        unsigned int hashIndex = (unsigned int)(-1);
        if (oldReportedAddress != au->reportedAddress)
        {
            // Remove this allocation unit from the hash table
            {
                size_t hashIndex = (((size_t)oldReportedAddress) >> 4) & (hashSize - 1);
                if (hashTable[hashIndex] == au)
                {
                    hashTable[hashIndex] = hashTable[hashIndex]->next;
                }
                else
                {
                    if (au->prev)
                        au->prev->next = au->next;
                    if (au->next)
                        au->next->prev = au->prev;
                }
            }

            // Re-insert it back into the hash table

            hashIndex = (((size_t)au->reportedAddress) >> 4) & (hashSize - 1);
            if (hashTable[hashIndex])
                hashTable[hashIndex]->prev = au;
            au->next = hashTable[hashIndex];
            au->prev = NULL;
            hashTable[hashIndex] = au;
        }

        // Account for the new allocatin unit in our stats

        stats.totalReportedMemory += (unsigned int)(au->reportedSize);
        stats.totalActualMemory += (unsigned int)(au->actualSize);
        if (stats.totalReportedMemory > stats.peakReportedMemory)
            stats.peakReportedMemory = stats.totalReportedMemory;
        if (stats.totalActualMemory > stats.peakActualMemory)
            stats.peakActualMemory = stats.totalActualMemory;
        int deltaReportedSize = (int)(reportedSize - originalReportedSize);
        if (deltaReportedSize > 0)
        {
            stats.accumulatedReportedMemory += deltaReportedSize;
            stats.accumulatedActualMemory += deltaReportedSize;
        }

        // Prepare the allocation unit for use (wipe it with recognizable garbage)

        wipeWithPattern(au, unusedPattern, originalReportedSize);

        // If you hit this assert, then something went wrong, because the allocation unit was properly validated PRIOR to
        // the reallocation. This should not happen.
        m_assert(mmgrValidateAllocUnit(au));

        // Validate every single allocated unit in memory

        if (alwaysValidateAll)
            mmgrValidateAllAllocUnits();

        // Log the result

        if (alwaysLogAll)
            Log("[~] ---->             addr 0x%08zX", (size_t)(au->reportedAddress));

        // Resetting the globals insures that if at some later time, somebody calls our memory manager from an unknown
        // source (i.e. they didn't include our H file) then we won't think it was the last allocation.

        resetGlobals();

        // Return the (reported) address of the new allocation unit

#ifdef TEST_MEMORY_MANAGER
        Log("[D] EXIT : mmgrReallocator()");
#endif

        MUTEX_UNLOCK(allocMutex);
        return au->reportedAddress;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Deallocate memory and track it
// ---------------------------------------------------------------------------------------------------------------------------------

void mmgrDeallocator(const char* sourceFile, const unsigned int sourceLine, const char* sourceFunc, const unsigned int deallocationType,
                     const void* reportedAddress)
{
    /*if (!allocMutex)
    allocMutex = CreateMutex();

    allocMutex->lock();*/
    MUTEX_LOCK(allocMutex);
#ifdef TEST_MEMORY_MANAGER
    Log("[D] ENTER: mmgrDeallocator()");
#endif

    // Log the request

    if (alwaysLogAll)
        Log("[-] ----- %8s of addr 0x%08zX           by %s", allocationTypes[deallocationType], (size_t)((void*)(reportedAddress)),
            ownerString(sourceFile, sourceLine, sourceFunc));

    // We should only ever get here with a null pointer if they try to do so with a call to free() (delete[] and delete will
    // both bail before they get here.) So, since ANSI allows free(NULL), we'll not bother trying to actually free the allocated
    // memory or track it any further.

    if (reportedAddress)
    {
        // Go get the allocation unit

        sAllocUnit* au = findAllocUnit(reportedAddress);

        // If you hit this assert, you tried to deallocate RAM that wasn't allocated by this memory manager.
        m_assert(au != NULL);
        if (au == NULL)
        {
            printf("Request to deallocate RAM that was naver allocated");
            fflush(stdout);
            MUTEX_UNLOCK(allocMutex);
            m_assert(false && "Request to deallocate RAM that was never allocated");
        }

        // If asan is active then unpoision the memory that may have been poisoned by another
        // library. Otherwise we trip asan here during the validation steps
        ASAN_UNPOISON(au->actualAddress, au->actualSize);

        // If you hit this assert, then the allocation unit that is about to be deallocated is damaged. But you probably
        // already know that from a previous assert you should have seen in validateAllocUnit() :)
        m_assert(mmgrValidateAllocUnit(au));

        // If you hit this assert, then this deallocation was made from a source that isn't setup to use this memory
        // tracking software, use the stack frame to locate the source and include our H file.
        m_assert(deallocationType != m_alloc_unknown);

        // If you hit this assert, you were trying to deallocate RAM that was not allocated in a way that is compatible with
        // the deallocation method requested. In other words, you have a allocation/deallocation mismatch.
        m_assert((deallocationType == m_alloc_delete && au->allocationType == m_alloc_new) ||
                 (deallocationType == m_alloc_delete_array && au->allocationType == m_alloc_new_array) ||
                 (deallocationType == m_alloc_free && au->allocationType == m_alloc_malloc) ||
                 (deallocationType == m_alloc_free && au->allocationType == m_alloc_calloc) ||
                 (deallocationType == m_alloc_free && au->allocationType == m_alloc_realloc) || (deallocationType == m_alloc_unknown));

        // If you hit this assert, then the "break on dealloc" flag for this allocation unit is set. Interrogate the 'au'
        // variable to determine information about this allocation unit.
        m_assert(au->breakOnDealloc == false);

        // Wipe the deallocated RAM with a new pattern. This doen't actually do us much good in debug mode under WIN32,
        // because Microsoft's memory debugging & tracking utilities will wipe it right after we do. Oh well.

        wipeWithPattern(au, releasedPattern, 0);

        // Inform asan that the memory is poisoned (again). Free should handle this for us but
        // we might as well
        ASAN_POISON(au->actualAddress, au->actualSize);

        // Do the deallocation

        free(au->actualAddress);

        // Remove this allocation unit from the hash table

        size_t hashIndex = ((size_t)(au->reportedAddress) >> 4) & (hashSize - 1);
        if (hashTable[hashIndex] == au)
        {
            hashTable[hashIndex] = au->next;
        }
        else
        {
            if (au->prev)
                au->prev->next = au->next;
            if (au->next)
                au->next->prev = au->prev;
        }

        // Remove this allocation from our stats

        stats.totalReportedMemory -= (unsigned int)(au->reportedSize);
        stats.totalActualMemory -= (unsigned int)(au->actualSize);
        stats.totalAllocUnitCount--;

        // Add this allocation unit to the front of our reservoir of unused allocation units

        memset(au, 0, sizeof(sAllocUnit));
        au->next = reservoir;
        reservoir = au;
    }

    // Resetting the globals insures that if at some later time, somebody calls our memory manager from an unknown
    // source (i.e. they didn't include our H file) then we won't think it was the last allocation.

    resetGlobals();

    // Validate every single allocated unit in memory

    if (alwaysValidateAll)
        mmgrValidateAllAllocUnits();

#ifdef TEST_MEMORY_MANAGER
    Log("[D] EXIT : mmgrDeallocator()");
#endif
    MUTEX_UNLOCK(allocMutex);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- The following utilitarian allow you to become proactive in tracking your own memory, or help you narrow in on those tough
// bugs.
// ---------------------------------------------------------------------------------------------------------------------------------

bool mmgrValidateAddress(const void* reportedAddress)
{
    // Just see if the address exists in our allocation routines

    return findAllocUnit(reportedAddress) != NULL;
}

// ---------------------------------------------------------------------------------------------------------------------------------

bool mmgrValidateAllocUnit(const sAllocUnit* allocUnit)
{
    // Make sure the padding is untouched

    uint8_t*     pre = ((uint8_t*)allocUnit->reportedAddress - paddingSize * sizeof(uint32_t));
    uint8_t*     post = ((uint8_t*)allocUnit->reportedAddress + allocUnit->reportedSize);
    bool         errorFlag = false;
    const size_t paddingBytes = paddingSize * sizeof(uint32_t);
    for (size_t i = 0; i < paddingBytes; i++, pre++, post++)
    {
        const uint8_t expectedPrefixByte = (prefixPattern >> ((i % sizeof(uint32_t)) * 8)) & 0xFF;
        if (*pre != expectedPrefixByte)
        {
            Log("[!] A memory allocation unit was corrupt because of an underrun:");
            mmgrDumpAllocUnit(allocUnit, "  ");
            errorFlag = true;
        }

        // If you hit this assert, then you should know that this allocation unit has been damaged. Something (possibly the
        // owner?) has underrun the allocation unit (modified a few bytes prior to the start). You can interrogate the
        // variable 'allocUnit' to see statistics and information about this damaged allocation unit.
        m_assert(*pre == expectedPrefixByte);

        const uint8_t expectedPostfixByte = (postfixPattern >> ((i % sizeof(uint32_t)) * 8)) & 0xFF;
        if (*post != expectedPostfixByte)
        {
            Log("[!] A memory allocation unit was corrupt because of an overrun:");
            mmgrDumpAllocUnit(allocUnit, "  ");
            errorFlag = true;
        }

        // If you hit this assert, then you should know that this allocation unit has been damaged. Something (possibly the
        // owner?) has overrun the allocation unit (modified a few bytes after the end). You can interrogate the variable
        // 'allocUnit' to see statistics and information about this damaged allocation unit.
        m_assert(*post == expectedPostfixByte);
    }

    // Return the error status (we invert it, because a return of 'false' means error)

    return !errorFlag;
}

// ---------------------------------------------------------------------------------------------------------------------------------

bool mmgrValidateAllAllocUnits(void)
{
    // Just go through each allocation unit in the hash table and count the ones that have errors

    unsigned int errors = 0;
    unsigned int allocCount = 0;
    for (unsigned int i = 0; i < hashSize; i++)
    {
        sAllocUnit* ptr = hashTable[i];
        while (ptr)
        {
            allocCount++;
            if (!mmgrValidateAllocUnit(ptr))
                errors++;
            ptr = ptr->next;
        }
    }

    // Test for hash-table correctness

    if (allocCount != stats.totalAllocUnitCount)
    {
        Log("[!] Memory tracking hash table corrupt!");
        errors++;
    }

    // If you hit this assert, then the internal memory (hash table) used by this memory tracking software is damaged! The
    // best way to track this down is to use the alwaysLogAll flag in conjunction with STRESS_TEST macro to narrow in on the
    // offending code. After running the application with these settings (and hitting this assert again), interrogate the
    // memory.log file to find the previous successful operation. The corruption will have occurred between that point and this
    // assertion.
    m_assert(allocCount == stats.totalAllocUnitCount);

    // If you hit this assert, then you've probably already been notified that there was a problem with a allocation unit in a
    // prior call to validateAllocUnit(), but this assert is here just to make sure you know about it. :)
    m_assert(errors == 0);

    // Log any errors

    if (errors)
        Log("[!] While validting all allocation units, %d allocation unit(s) were found to have problems", errors);

    // Return the error status

    return errors != 0;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- Unused RAM calculation routines. Use these to determine how much of your RAM is unused (in bytes)
// ---------------------------------------------------------------------------------------------------------------------------------

unsigned int mmgrCalcUnused(const sAllocUnit* allocUnit)
{
    const uint32_t* ptr = (const uint32_t*)(allocUnit->reportedAddress);
    unsigned int    count = 0;

    for (unsigned int i = 0; i < allocUnit->reportedSize; i += sizeof(uint32_t), ptr++)
    {
        if (*ptr == unusedPattern)
            count += sizeof(uint32_t);
    }

    return count;
}

// ---------------------------------------------------------------------------------------------------------------------------------

unsigned int mmgrCalcAllUnused(void)
{
    // Just go through each allocation unit in the hash table and count the unused RAM

    unsigned int total = 0;
    for (unsigned int i = 0; i < hashSize; i++)
    {
        sAllocUnit* ptr = hashTable[i];
        while (ptr)
        {
            total += mmgrCalcUnused(ptr);
            ptr = ptr->next;
        }
    }

    return total;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// -DOC- The following functions are for logging and statistics reporting.
// ---------------------------------------------------------------------------------------------------------------------------------

void mmgrDumpAllocUnit(const sAllocUnit* allocUnit, const char* prefix)
{
    Log("[I] %sAddress (reported): %010p", prefix, allocUnit->reportedAddress);
    Log("[I] %sAddress (actual)  : %010p", prefix, allocUnit->actualAddress);
    Log("[I] %sSize (reported)   : 0x%08X (%s)", prefix, (unsigned int)(allocUnit->reportedSize),
        memorySizeString((unsigned int)(allocUnit->reportedSize)));
    Log("[I] %sSize (actual)     : 0x%08X (%s)", prefix, (unsigned int)(allocUnit->actualSize),
        memorySizeString((unsigned int)(allocUnit->actualSize)));
    Log("[I] %sOwner             : %s(%d)::%s", prefix, allocUnit->sourceFile, allocUnit->sourceLine, allocUnit->sourceFunc);

#if MMGR_BACKTRACE
    Log("[I] %sBacktrace             : ", prefix);
    dumpBacktrace(NULL, allocUnit);
#endif

    Log("[I] %sAllocation type   : %s", prefix, allocationTypes[allocUnit->allocationType]);
    Log("[I] %sAllocation number : %d", prefix, allocUnit->allocationNumber);
}

// ---------------------------------------------------------------------------------------------------------------------------------
static void fsPrintf(FileStream* fileStream, const char* format, ...)
{
    va_list args;
    char    buffer[BUFFER_SIZE] = { 0 };
    va_start(args, format);
    vsnprintf(buffer, BUFFER_SIZE, format, args);
    va_end(args);
    fsWriteToStream(fileStream, buffer, strlen(buffer));
}

void mmgrDumpMemoryReport(const char* filename, const bool overwrite)
{
    {
        FileStream fh = { 0 };
        bool       success = fsOpenStreamFromPath(RD_LOG, filename, overwrite ? FM_WRITE : FM_APPEND, &fh);

        // If you hit this assert, then the memory report generator is unable to log information to a file (can't open the file for
        // some reason.)
        if (!success)
            return;

        // Header
        static char timeString[25];
        memset(timeString, 0, sizeof(timeString));
        time_t    t = time(NULL);
        struct tm tme;
#ifdef _WIN32
        localtime_s(&tme, &t);
#else
        localtime_s(&t, &tme);
#endif

        fsPrintf(
            &fh,
            " -----------------------------------------------------------------------------------------------------------------------------"
            "-----\n");
        fsPrintf(
            &fh,
            "|                                             Memory report for: %02d/%02d/%04d %02d:%02d:%02d                                "
            "          |\n",
            tme.tm_mon + 1, tme.tm_mday, tme.tm_year + 1900, tme.tm_hour, tme.tm_min, tme.tm_sec);
        fsPrintf(
            &fh,
            " -----------------------------------------------------------------------------------------------------------------------------"
            "-----\n");
        fsPrintf(&fh, "\n");

        // Report summary
        fsPrintf(
            &fh,
            " -----------------------------------------------------------------------------------------------------------------------------"
            "----- \n");
        fsPrintf(
            &fh,
            "|                                                           T O T A L S                                                       "
            "     |\n");
        fsPrintf(
            &fh,
            " -----------------------------------------------------------------------------------------------------------------------------"
            "----- \n");
        fsPrintf(&fh, "              Allocation unit count: %10s\n", insertCommas(stats.totalAllocUnitCount));
        fsPrintf(&fh, "            Reported to application: %s\n", memorySizeString(stats.totalReportedMemory));
        fsPrintf(&fh, "         Actual total memory in use: %s\n", memorySizeString(stats.totalActualMemory));
        fsPrintf(&fh, "           Memory tracking overhead: %s\n", memorySizeString(stats.totalActualMemory - stats.totalReportedMemory));
        fsPrintf(&fh, "\n");

        fsPrintf(
            &fh,
            " -----------------------------------------------------------------------------------------------------------------------------"
            "----- \n");
        fsPrintf(
            &fh,
            "|                                                            P E A K S                                                        "
            "     |\n");
        fsPrintf(
            &fh,
            " -----------------------------------------------------------------------------------------------------------------------------"
            "----- \n");
        fsPrintf(&fh, "              Allocation unit count: %10s\n", insertCommas(stats.peakAllocUnitCount));
        fsPrintf(&fh, "            Reported to application: %s\n", memorySizeString(stats.peakReportedMemory));
        fsPrintf(&fh, "                             Actual: %s\n", memorySizeString(stats.peakActualMemory));
        fsPrintf(&fh, "           Memory tracking overhead: %s\n", memorySizeString(stats.peakActualMemory - stats.peakReportedMemory));
        fsPrintf(&fh, "\n");

        fsPrintf(
            &fh,
            " -----------------------------------------------------------------------------------------------------------------------------"
            "----- \n");
        fsPrintf(
            &fh,
            "|                                                      A C C U M U L A T E D                                                  "
            "     |\n");
        fsPrintf(
            &fh,
            " -----------------------------------------------------------------------------------------------------------------------------"
            "----- \n");
        fsPrintf(&fh, "              Allocation unit count: %s\n", memorySizeString(stats.accumulatedAllocUnitCount));
        fsPrintf(&fh, "            Reported to application: %s\n", memorySizeString(stats.accumulatedReportedMemory));
        fsPrintf(&fh, "                             Actual: %s\n", memorySizeString(stats.accumulatedActualMemory));
        fsPrintf(&fh, "\n");

        fsPrintf(
            &fh,
            " -----------------------------------------------------------------------------------------------------------------------------"
            "----- \n");
        fsPrintf(
            &fh,
            "|                                                           U N U S E D                                                       "
            "     |\n");
        fsPrintf(
            &fh,
            " -----------------------------------------------------------------------------------------------------------------------------"
            "----- \n");
        fsPrintf(&fh, "Memory allocated but not in use: %s\n", memorySizeString(mmgrCalcAllUnused()));
        fsPrintf(&fh, "\n");

        dumpAllocations(&fh);

        fsCloseStream(&fh);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------

sMStats mmgrGetMemoryStatistics(void) { return stats; }

#include "nommgr.h"
char* LogToMemory(char* log)
{
    // static char* logMemory = (char*)calloc(1, sizeof(char));
    static char* logMemory = NULL;
    if (logMemory == NULL)
    {
        logMemory = (char*)calloc(1, sizeof(char));
    }
    static size_t memoryLength = 1;

    size_t logLength = strlen(log) + 1;

    logMemory = (char*)realloc(logMemory, memoryLength + logLength - 1);
    memcpy(logMemory + memoryLength - 1, log, logLength);

    memoryLength += logLength - 1;

    return logMemory;
}

MUTEX* CreateMutex(void)
{
    MUTEX* mutex = (MUTEX*)malloc(sizeof(MUTEX));
    initMutex(mutex);
    return mutex;
}

void RemoveMutex(MUTEX** mutex)
{
    if (*mutex)
    {
        destroyMutex(*mutex);
        free(*mutex);
        *mutex = NULL;
    }
}
// ---------------------------------------------------------------------------------------------------------------------------------
// mmgr.cpp - End of file
// ---------------------------------------------------------------------------------------------------------------------------------
