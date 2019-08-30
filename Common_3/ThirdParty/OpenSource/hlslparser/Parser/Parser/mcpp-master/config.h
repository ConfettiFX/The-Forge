// **********************************************************************
//
// Copyright (c) 2015 ZeroC, Inc. All rights reserved.
//
// **********************************************************************

// Simplified and reduced version of config.h, with support for Windows,
// OS X and Linux.

#define COMPILER INDEPENDENT

// Windows support for MSC and MINGW
#if defined(_WIN32)

#define HOST_COMPILER MSC
#define HOST_SYSTEM SYS_WIN
#define SYSTEM SYS_WIN
#define OBJEXT "obj"

#elif defined(__APPLE__)

/* Define if the cases of file name are folded. */
#define FNAME_FOLD 1

/* Define to 1 if the system has the type `intmax_t'. */
#define HAVE_INTMAX_T 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if the system has the type `long long'. */
#define HAVE_LONG_LONG 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the `stpcpy' function. */
#define HAVE_STPCPY 1

/* Define the host compiler. */
#define HOST_COMPILER GNUC

/* Define the host system. */
#define HOST_SYSTEM SYS_MAC

/* Define printf length modifier for the longest integer. */
#define LL_FORM "j"

/* Define the suffix of object file. */
#define OBJEXT "o"

/* Define the target system. */
#define SYSTEM SYS_MAC

#else

// Linux.

/* Define to 1 if the system has the type `intmax_t'. */
#define HAVE_INTMAX_T 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if the system has the type `long long'. */
#define HAVE_LONG_LONG 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the `stpcpy' function. */
#define HAVE_STPCPY 1

/* Define the host compiler. */
#define HOST_COMPILER GNUC

/* Define the host system. */
#define HOST_SYSTEM SYS_LINUX

/* Define printf length modifier for the longest integer. */
#define LL_FORM "j"

/* Define the suffix of object file. */
#define OBJEXT "o"

/* Define the target system. */
#define SYSTEM SYS_LINUX

#endif
