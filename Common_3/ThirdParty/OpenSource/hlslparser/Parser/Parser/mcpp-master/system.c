/*-
 * Copyright (c) 1998, 2002-2008 Kiyoshi Matsui <kmatsui@t3.rim.or.jp>
 * All rights reserved.
 *
 * Some parts of this code are derived from the public domain software
 * DECUS cpp (1984,1985) written by Martin Minow.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 *                          S Y S T E M . C
 *          S y s t e m   D e p e n d e n t   R o u t i n e s
 *
 * Routines dependent on O.S., compiler or compiler-driver.
 * To port MCPP for the systems not yet ported, you must
 *      1. specify the constants in "configed.H" or "noconfig.H",
 *      2. append the system-dependent routines in this file.
 */

#pragma warning(disable:4996)
#pragma warning(disable:4267)

#include    "system.H"
#include    "internal.H"

#if     HOST_SYS_FAMILY == SYS_UNIX
#include    "unistd.h"              /* For getcwd(), readlink() */
#elif   HOST_COMPILER == MSC || HOST_COMPILER == LCC
#include    "direct.h"
#define getcwd( buf, size)  _getcwd( buf, size)
#elif   HOST_COMPILER == BORLANDC
#include    "dir.h"
#endif

#include    "sys/types.h"
#include    "sys/stat.h"                        /* For stat()       */
#if     ! defined( S_ISREG)
#define S_ISREG( mode)  (mode & S_IFREG)
#define S_ISDIR( mode)  (mode & S_IFDIR)
#endif
#if     HOST_COMPILER == MSC
#define S_IFREG     _S_IFREG
#define S_IFDIR     _S_IFDIR
#define stat( path, stbuf)  _stat( path, stbuf)
#endif

/* Function to compare path-list    */
#if     FNAME_FOLD
#if     HOST_COMPILER == GNUC   /* CYGWIN, MINGW, MAC   */
#include    <strings.h>         /* POSIX 1, 2001        */
#define str_case_eq( str1, str2)    (strcasecmp( str1, str2) == 0)
#else   /* MSC, BORLANDC, LCC   */
#if     HOST_COMPILER == MSC
#define stricmp( str1, str2)        _stricmp( str1, str2)
#endif
#define str_case_eq( str1, str2)    (stricmp( str1, str2) == 0)
#endif
#else   /* ! FNAME_FOLD */
#define str_case_eq( str1, str2)    (strcmp( str1, str2) == 0)
#endif

/*
 * PATH_DELIM is defined for the O.S. which has single byte path-delimiter.
 * Note: '\\' or any other character identical to second byte of MBCHAR should
 * not be used for PATH_DELIM for convenience of path-list parsing.
 */
#if SYS_FAMILY == SYS_UNIX || SYS_FAMILY == SYS_WIN || SYSTEM == SYS_UNKNOWN
#define PATH_DELIM      '/'
#define SPECIAL_PATH_DELIM  FALSE
#else   /* Any other path-delimiter, define PATH_DELIM by yourself  */
#define SPECIAL_PATH_DELIM  TRUE    /* Any path-delimiter other than '/'    */
#endif

/*
 * OBJEXT is the suffix to denote "object" file.
 */
#ifndef OBJEXT
#if     SYS_FAMILY == SYS_UNIX || HOST_COMPILER == GNUC
#define OBJEXT     "o"
#elif   SYS_FAMILY == SYS_WIN
#define OBJEXT     "obj"
#elif   1
/* Add here appropriate definitions for other systems.  */
#endif
#endif

static void     version( void);
                /* Print version message            */
static void     usage( int opt);
                /* Putout usage of MCPP             */
static void     set_opt_list( char * optlist);
                /* Set list of legal option chars   */
static void     def_a_macro( int opt, char * def);
                /* Do a -D option                   */
static void     chk_opts( int sflag, int trad);
                /* Check consistency of options     */
static void     init_predefines( void);
                /* Set and unset predefined macros  */
static void     init_std_defines( void);
                /* Predefine Standard macros        */
static void     set_limit( void);
                /* Set minimum translation limits   */

static void     put_info( FILEINFO * sharp_file);
                /* Print compiler-specific-inf      */
static char *   set_files( int argc, char ** argv, char ** in_pp
        , char ** out_pp);
                /* Set input, output, diagnostic    */
static void     set_sys_dirs( int set_cplus_dir);
                /* Set system-specific include dirs */
static void     set_env_dirs( void);
                /* Set user-defined include dirs    */
static void     parse_env( const char * env);
                /* Parse environment variables      */
static void     set_a_dir( const char * dirname);
                /* Append an include directory      */
static char *   norm_dir( const char * dirname, int framework);
                /* Normalize include directory path */
static char *   norm_path( const char * dir, const char * fname, int inf
        , int hmap);    /* Normalize pathname to compare    */
#if SYS_FAMILY == SYS_UNIX
static void     deref_syml( char * slbuf1, char * slbuf2, char * chk_start);
                /* Dereference symbolic linked directory and file   */
#endif
static void     def_macros( void);
                /* Define macros specified by -D    */
static void     undef_macros( void);
                /* Undefine macros specified by -U  */
static char *   md_init( const char * filename, char * output);
                /* Initialize makefile dependency   */
static char *   md_quote( char * output);
                /* 'Quote' special characters       */
static int      open_include( char * filename, int searchlocal, int next);
                /* Open the file to include         */
static int      has_directory( const char * source, char * directory);
                /* Get directory part of fname      */
static int      is_full_path( const char * path);
                /* The path is absolute path list ? */
static int      search_dir( char * filename, int searchlocal, int next);
                /* Search the include directories   */
static int      open_file( const char ** dirp, const char * src_dir
        , const char * filename, int local, int include_opt, int sys_frame);
                /* Open a source file       */
static const char *     set_fname( const char * filename);
                /* Remember the source filename     */
#if 0   /* This function is only for debugging use  */
static int      chk_dirp( const char ** dirp);
                /* Check validity of dirp arg for open_file()   */
#endif
static void     cur_file( FILEINFO * file, FILEINFO * sharp_file, int marker);
                /* Output current source file name  */
#if SYS_FAMILY == SYS_WIN
static char *   bsl2sl( char * filename);
                /* Convert \ to / in path-list      */
#endif
static int      is_junk( void);
                /* The directive has trailing junk? */
static void     do_once( const char * fullname);
                /* Process #pragma once             */
static int      included( const char * fullname);
                /* The file has been once included? */
static void     push_or_pop( int direction);
                /* Push or pop a macro definition   */
static void     do_preprocessed( void);
                /* Process preprocessed file        */
static int      do_debug( int set);
                /* #pragma MCPP debug, #debug       */
static void     dump_path( void);
                /* Print include search path        */
static int      mcpp_getopt( int argc, char * const * argv, const char * opts);
                /* getopt() to prevent linking of glibc getopt  */

/* for mcpp_getopt()    */
static int      mcpp_optind = 1;
static int      mcpp_opterr = 1;
static int      mcpp_optopt;
static char *   mcpp_optarg;

static int      mb_changed = FALSE;     /* Flag of -e option        */
static char     cur_work_dir[ PATHMAX + 1];     /* Current working directory*/

/*
 * incdir[] stores the -I directories (and the system-specific #include <...>
 * directories).  This is set by set_a_dir().  A trailing PATH_DELIM is
 * appended if absent.
 */
static const char **    incdir;         /* Include directories      */
static const char **    incend;         /* -> active end of incdir  */
static int          max_inc;            /* Number of incdir[]       */

typedef struct inc_list {       /* List of directories or files     */
    char       *name;           /* Filename or directory-name       */
    size_t      len;                    /* Length of 'name'         */
} INC_LIST;

/*
 * fnamelist[] stores the souce file names opened by #include directive for
 * debugging information.
 */
static INC_LIST *   fnamelist;          /* Source file names        */
static INC_LIST *   fname_end;          /* -> active end of fnamelist   */
static int          max_fnamelist;      /* Number of fnamelist[]    */

/* once_list[] stores the #pragma once file names.  */
static INC_LIST *   once_list;          /* Once opened file         */
static INC_LIST *   once_end;           /* -> active end of once_list   */
static int          max_once;           /* Number of once_list[]    */

#define INIT_NUM_INCLUDE    32          /* Initial number of incdir[]   */
#define INIT_NUM_FNAMELIST  256         /* Initial number of fnamelist[]    */
#define INIT_NUM_ONCE       64          /* Initial number of once_list[]    */

/*
 * 'search_rule' holds searching rule of #include "header.h" to search first
 * before searching user specified or system-specific include directories.
 * 'search_rule' is initialized to SEARCH_INIT.  It can be changed by -I1, -I2
 * or -I3 option.  -I1 specifies CURRENT, -I2 SOURCE and -I3 both.
 */

static int      search_rule = SEARCH_INIT;  /* Rule to search include file  */

static int      nflag = FALSE;          /* Flag of -N (-undef) option       */
static long     std_val = -1L;  /* Value of __STDC_VERSION__ or __cplusplus */

#define MAX_DEF   256
#define MAX_UNDEF (MAX_DEF/4)
static char *   def_list[ MAX_DEF];     /* Macros to be defined     */
static char *   undef_list[ MAX_UNDEF]; /* Macros to be undefined   */
static int      def_cnt;                /* Count of def_list        */
static int      undef_cnt;              /* Count of undef_list      */

/* Values of mkdep. */
#define MD_MKDEP        1   /* Output source file dependency line   */
#define MD_SYSHEADER    2   /* Print also system-header names       */
#define MD_FILE         4   /* Output to the file named *.d         */
#define MD_PHONY        8   /* Print also phony targets for each header */
#define MD_QUOTE        16  /* 'Quote' $ and space in target name   */

static FILE *   mkdep_fp;                       /* For -Mx option   */
static char *   mkdep_target;
    /* For -MT TARGET option and for GCC's queer environment variables.     */
static char *   mkdep_mf;               /* Argument of -MF option   */
static char *   mkdep_md;               /* Argument of -MD option   */
static char *   mkdep_mq;               /* Argument of -MQ option   */
static char *   mkdep_mt;               /* Argument of -MT option   */

/* sharp_filename is filename for #line line, used only in cur_file()   */
static char *   sharp_filename = NULL;
static char *   argv0;      /* argv[ 0] for usage() and version()   */
static int      compat_mode;
                /* "Compatible" mode of recursive macro expansion   */

#if SYSTEM == SYS_CYGWIN
static int      no_cygwin = FALSE;          /* -mno-cygwin          */

#endif

#define NO_DIR  FALSE
#if NO_DIR
/* Unofficial feature to strip directory part of include file   */
static int      no_dir;
#endif

void    init_system( void)
/* Initialize static variables  */
{
    if (sharp_filename)
        free( sharp_filename);
    sharp_filename = NULL;
    incend = incdir = NULL;
    fnamelist = once_list = NULL;
    search_rule = SEARCH_INIT;
    mb_changed = nflag = compat_mode = FALSE;
    mkdep_fp = NULL;
    mkdep_target = mkdep_mf = mkdep_md = mkdep_mq = mkdep_mt = NULL;
    std_val = -1L;
    def_cnt = undef_cnt = 0;
    mcpp_optind = mcpp_opterr = 1;
#if SYSTEM == SYS_CYGWIN
    no_cygwin = FALSE;
#endif
#if NO_DIR
    no_dir = FALSE;
#endif
}


#define OPTLISTLEN  80

void    do_options(
    int         argc,
    char **     argv,
    char **     in_pp,                      /* Input file name      */
    char **     out_pp                      /* Output file name     */
)
/*
 * Process command line arguments, called only at MCPP startup.
 */
{
    char        optlist[ OPTLISTLEN];       /* List of option letter*/
    int         opt;
    int         unset_sys_dirs;
        /* Unset system-specific and site-specific include directories ?    */
    int         set_cplus_dir;  /* Set C++ include directory ? (for GCC)*/
    int         show_path;          /* Show include directory list  */
    int         sflag;                      /* -S option or similar */
    int         trad;                       /* -traditional         */
    int         i;

    argv0 = argv[ 0];
    nflag = unset_sys_dirs = show_path = sflag = trad = FALSE;
    set_cplus_dir = TRUE;

    /* Get current directory for -I option and #pragma once */
    getcwd( cur_work_dir, PATHMAX);
#if SYS_FAMILY == SYS_WIN
    bsl2sl( cur_work_dir);
#endif
    sprintf( cur_work_dir + strlen( cur_work_dir), "%c%c", PATH_DELIM, EOS);
        /* Append trailing path-delimiter   */

    set_opt_list( optlist);

opt_search: ;
    while (mcpp_optind < argc
            && (opt = mcpp_getopt( argc, argv, optlist)) != EOF) {

        switch (opt) {          /* Command line option character    */

        case 'C':                           /* Keep comments        */
            option_flags.c = TRUE;
            break;

        case 'D':                           /* Define symbol        */
            if (def_cnt >= MAX_DEF) {
                mcpp_fputs( "Too many -D options.\n", ERR);
                longjmp( error_exit, -1);
            }
            def_list[ def_cnt++] = mcpp_optarg;
            break;

        case 'e':
            // We support this option, but it does nothing.
            break;

        case 'I':                           /* Include directory    */
            if (str_eq( mcpp_optarg, "-")) {        /* -I-                  */
                unset_sys_dirs = TRUE;
                        /* Unset pre-specified include directories  */
            } else if (*(mcpp_optarg + 1) == EOS && isdigit( *mcpp_optarg)
                    && (i = *mcpp_optarg - '0') != 0
                    && (i & ~(CURRENT | SOURCE)) == 0) {
                search_rule = i;            /* -I1, -I2 or -I3      */
            } else {                        /* Not '-' nor a digit  */
                set_a_dir( mcpp_optarg);    /* User-defined dir     */
            }
            break;


        case 'M':           /* Output source file dependency line   */
            if (argv[ mcpp_optind - 1] == mcpp_optarg) {     /* -M   */
                mkdep |= MD_SYSHEADER;
                mcpp_optind--;
            } else {
                usage( opt);
            }
            mkdep |= MD_MKDEP;
            break;

        case 'U':                           /* Undefine macro       */
            if (undef_cnt >= MAX_UNDEF) {
                mcpp_fputs( "Too many -U options.\n", ERR);
                longjmp( error_exit, -1);
            }
            undef_list[ undef_cnt++] = mcpp_optarg;
            break;

        default:                            /* What is this one?    */
            usage( opt);
            break;
        }                               /* Switch on all options    */

    }                                   /* For all arguments        */

    if (mcpp_optind < argc && set_files( argc, argv, in_pp, out_pp) != NULL)
        goto  opt_search;       /* More options after the filename  */

    /* Check consistency of specified options, set some variables   */
    chk_opts( sflag, trad);

    if (warn_level == -1)               /* No -W option             */
        warn_level = 1;                 /* Default warning level    */
    else if (warn_level == 0xFF)
        warn_level = 0;                 /* -W0 has high precedence  */

	/* FORGE CHANGE */
	/* -W option seems to be missing. */
	/* The 0x01 warnings are trivial so ignore them. Like no \n at the last line of a file. */
	/* The 0x02 warnings also include "Parsed // as comment", which is not helpful. */
	/* The 0x04 warnings include a message if a token is longer than 31 bytes, which is common and normal. */
	/* So we only care about the 0x08 warnings. */
	warn_level = 0xF8;

    set_env_dirs();
    if (! unset_sys_dirs)
        set_sys_dirs( set_cplus_dir);

    if (mkdep_mf) {                         /* -MF overrides -MD    */
        mkdep_fp = mcpp_fopen( mkdep_mf, "w");
    } else if (mkdep_md) {
        mkdep_fp = mcpp_fopen( mkdep_md, "w");
    }
    if (mkdep_mq)                           /* -MQ overrides -MT    */
        mkdep_target = mkdep_mq;
    else if (mkdep_mt)
        mkdep_target = mkdep_mt;

    /* Normalize the path-list  */
    if (*in_pp && ! str_eq( *in_pp, "-")) {
        char *  tmp = norm_path( null, *in_pp, FALSE, FALSE);
        if (tmp)                        /* The file exists          */
            *in_pp = tmp;
            /* Else mcpp_main() will diagnose *in_pp and exit   */
    }
    if (! (mcpp_debug & MACRO_CALL)) {
        if (show_path) {
            fp_debug = stderr;
            dump_path();
            fp_debug = stdout;
        }
    }
}

static void version( void)
/*
 * Print version message.
 */
{
    const char *    mes[] = {
/* Write messages here, for example, "MySomeTool with ".    */
        "MCPP V.2.7.2 (2008/11)"
        , "https://github.com/zeroc-ice/mcpp"
        , "compiler-independent-build "
        , NULL
    };

    const char **   mpp = mes;
    while (*mpp)
        mcpp_fputs( *mpp++, ERR);
}

static void usage(
    int     opt
)
/*
 * Print usage.
 */
{
    const char *     mes[] = {

"Usage:  ",
"mcpp",
" [-<opts> [-<opts>]] [<infile> [-<opts>] [<outfile>] [-<opts>]]\n",
"    <infile> defaults to stdin and <outfile> defaults to stdout.\n",

"\nCommonly used options:\n",

"-C          Output also comments.\n",
"-D <macro>[=<value>]    Define <macro> as <value> (default:1).\n",
"-D <macro(args)>[=<replace>]    Define <macro(args)> as <replace>.\n",
"-e <encoding>   Change the default multi-byte character encoding to one of:\n",
"            euc_jp, gb2312, ksc5601, big5, sjis, iso2022_jp, utf8.\n",

"-I <directory>      Add <directory> to the #include search list.\n",

"-I-         Unset system or site specific include directories.\n",

"-M file\n",
"            Output source file dependency line for makefile.\n",
"-U <macro>  Undefine <macro>.\n",

"\nFor further details see mcpp-manual.html.\n",
        NULL,
    };

    const char *    illegopt = "Incorrect option -%c%s\n";
    const char * const *    mpp = mes;

    if (opt != '?')
        mcpp_fprintf( ERR, illegopt, opt, mcpp_optarg ? mcpp_optarg : null);
    version();
    mes[ 1] = argv0;
    while (*mpp)
        mcpp_fputs( *mpp++, ERR);
    longjmp( error_exit, -1);
}

static void set_opt_list(
    char *  optlist
)
/*
 * Set list of legal option characters.
 */
{
    const char *    list[] = {

#if ! STD_LINE_PREFIX
    "b",
#endif

#if SYS_FAMILY == SYS_UNIX
    "m:",
#endif

    "a",

    NULL
    };

    const char * const *    lp = & list[ 0];

    strcpy( optlist, "e:CD:I:M:U:");
                                                /* Default options  */
    while (*lp)
        strcat( optlist, *lp++);
    if (strlen( optlist) >= OPTLISTLEN)
        cfatal( "Bug: Too long option list", NULL, 0L, NULL);       /* _F_  */
}

static void def_a_macro(
    int     opt,                            /* 'D'  */
    char *  def                         /* Argument of -D option    */
)
/*
 * Define a macro specified by -D option.
 * The macro maybe either object-like or function-like (with parameter).
 */
{
    DEFBUF *    defp;
    char *      definition;             /* Argument of -D option    */
    char *      cp;
    int         i;

    definition = xmalloc( strlen( def) + 4);
    strcpy( definition, def);
    if ((cp = strchr( definition, '=')) != NULL) {
        *cp = ' ';                          /* Remove the '='       */
        cp = "\n";                          /* Append <newline>     */
    } else {
        cp = " 1\n";                        /* With definition "1"  */
    }
    strcat( definition, cp);
    cp = definition;
    while ((char_type[ *cp & UCHARMAX] & SPA) == 0)
        cp++;
    i = *cp;
    *cp = EOS;
    if ((defp = look_id( definition)) != NULL)      /* Pre-defined  */
        undefine( definition);
    *cp = i;
    /* Now, save the definition.    */
    unget_string( definition, NULL);
    if (do_define( FALSE, 0) == NULL)       /* Define a macro       */
        usage( opt);
    *cp = EOS;
    if (str_eq( definition, "__STDC__")) {
        defp = look_id( definition);
        defp->nargs = DEF_NOARGS_STANDARD;
                                /* Restore Standard-predefinedness  */
    }
    free( definition);
    skip_nl();                      /* Clear the appended <newline> */
}

static void     chk_opts(
    int     sflag,      /* Flag of Standard or post-Standard mode   */
    int     trad                    /* -traditional (GCC only)      */
)
/*
 * Check consistency between the specified options.
 * Set default value of some variables for each 'mcpp_mode'.
 */
{
    int     incompat = FALSE;

    if (trad)
        incompat = TRUE;
    if (! stdc_val)
        stdc_val = STDC;

    if (compat_mode)
        incompat = TRUE;
    if ((mcpp_debug & MACRO_CALL)
                && option_flags.c) {
            mcpp_fputs( "Disabled -K option.\n", ERR);
            mcpp_debug &= ~MACRO_CALL;
            /* -a and -C options do not co-exist with -K    */
    }
    if (incompat) {
        mcpp_fputs( "Incompatible options are specified.\n", ERR);
        usage( '?');
    }

    expand_init( compat_mode);
                /* Set function pointer to macro expansion routine  */
}

static void init_predefines( void)
/*
 * Set or unset predefined macros.
 */
{
    char    tmp[ 16];

    stdc_ver = stdc_val ? STDC_VERSION : 0L;

    if (nflag) {
        un_predefine( TRUE);
    } else if (stdc_val) {
        un_predefine( FALSE);           /* Undefine "unix" or so    */
    }
    sprintf( tmp, "%ldL", stdc_ver);
    if (stdc_ver)
        look_and_install( "__STDC_VERSION__", DEF_NOARGS_STANDARD, null
                , tmp);
#ifdef  COMPILER_CPLUS
    if (! nflag)        /* Undefine pre-defined macro for C++   */
        undefine( COMPILER_CPLUS);
#endif
    set_limit();
    init_std_defines();
}

static void init_std_defines( void)
/*
 * For STD and POST_STD modes.
 * The magic pre-defines are initialized with magic argument counts.
 * expand_macro() notices this and calls the appropriate routine.
 * DEF_NOARGS is one greater than the first "magic" definition.
 * 'DEF_NOARGS - n' are reserved for pre-defined macros.
 * __STDC_VERSION__ and __cplusplus are defined by chk_opts() and set_cplus().
 */
{
    char    tmp[ 16];
    char    timestr[ 14];
    time_t  tvec;
    char *  tstring;

    look_and_install( "__LINE__", DEF_NOARGS_DYNAMIC - 1, null, "-1234567890");
    /* Room for 11 chars (10 for long and 1 for '-' in case of wrap round.  */
    look_and_install( "__FILE__", DEF_NOARGS_DYNAMIC - 2, null, null);
                                            /* Should be stuffed    */

    /* Define __DATE__, __TIME__ as present date and time.          */
    time( &tvec);
    tstring = ctime( &tvec);
    sprintf( timestr, "\"%.3s %c%c %.4s\"",
        tstring + 4,
        *(tstring + 8) == '0' ? ' ' : *(tstring + 8),
        *(tstring + 9),
        tstring + 20);
    look_and_install( "__DATE__", DEF_NOARGS_DYNAMIC, null, timestr);
    sprintf( timestr, "\"%.8s\"", tstring + 11);
    look_and_install( "__TIME__", DEF_NOARGS_DYNAMIC, null, timestr);

    if (! look_id( "__STDC_HOSTED__")) {
        /*
         * Some compilers, e.g. GCC older than 3.3, define this macro by
         * -D option.
         */
        sprintf( tmp, "%d", STDC_HOSTED);
        look_and_install( "__STDC_HOSTED__", DEF_NOARGS_PREDEF, null, tmp);
    }
    /* Define __STDC__ as 1 or such for Standard conforming compiler.   */
    if (! look_id( "__STDC__")) {
        sprintf( tmp, "%d", stdc_val);
        look_and_install( "__STDC__", DEF_NOARGS_STANDARD, null, tmp);
    }
}

#define LINE90LIMIT         32767
#define LINE_CPLUS_LIMIT    32767

static void set_limit( void)
/*
 * Set the minimum translation limits specified by the Standards.
 */
{
    /* Specified by C 1990 Standard */
    std_limits.str_len = SLEN90MIN;
    std_limits.id_len = IDLEN90MIN;
    std_limits.n_mac_pars = NMACPARS90MIN;
    std_limits.exp_nest = EXP_NEST90MIN;
    std_limits.blk_nest = BLK_NEST90MIN;
    std_limits.inc_nest = INCLUDE_NEST90MIN;
    std_limits.n_macro = NMACRO90MIN;
    std_limits.line_num = LINE90LIMIT;
}


void    init_sys_macro( void)
/*
 * Define system-specific macros and some Standard required macros
 * and undefine macros specified by -U options.
 */
{
    /* This order is important. */
    def_macros();               /* Define macros specified by -D    */
    init_predefines();                  /* Define predefined macros */
    undef_macros();             /* Undefine macros specified by -U  */
    if (mcpp_debug & MACRO_CALL)
        dump_def( FALSE, TRUE);     /* Finally putout current macro names   */
}

void    at_start( void)
/*
 * Do the commands prior to processing main source file after do_options().
 */
{
    FILEINFO *      file_saved = infile;

    // We don't support changing the encodings.

    put_info( file_saved);
}

static void put_info(
    FILEINFO *  sharp_file
)
/*
 * Putout compiler-specific information.
 */
{
    if (no_output)
        return;
    sharp_file->line++;
    sharp( sharp_file, 0);
    sharp_file->line--;
}

static char *   set_files(
    int     argc,
    char ** argv,
    char ** in_pp,
    char ** out_pp
)
/*
 * Set input and/or output files.
 */
{
    char *      cp;

    if (*in_pp == NULL) {                           /* Input file   */
        cp = argv[ mcpp_optind++];
#if SYS_FAMILY == SYS_WIN
        cp = bsl2sl( cp);
#endif
        *in_pp = cp;
    }
    if (mcpp_optind < argc && argv[ mcpp_optind][ 0] != '-'
            && *out_pp == NULL) {
        cp = argv[ mcpp_optind++];
#if SYS_FAMILY == SYS_WIN
        cp = bsl2sl( cp);
#endif
        *out_pp = cp;                               /* Output file  */
    }
    if (mcpp_optind >= argc)
        return  NULL;           /* Exhausted command line arguments */
    if (argv[ mcpp_optind][ 0] == '-')
        return  argv[ mcpp_optind];                 /* More options */
    cfatal( "Excessive file argument \"%s\"", argv[ mcpp_optind], 0L , NULL);
    return  NULL;
}

static void set_env_dirs( void)
/*
 * Add to include path those specified by environment variables.
 */
{
    const char *    env;

    if ((env = getenv( ENV_C_INCLUDE_DIR)) != NULL)
        parse_env( env);
}

static void parse_env(
    const char *    env
)
/*
 * Parse environmental variable and append the path to include-dir-list.
 */
{
    char *  save;
    char *  save_start;
    char *  p;
    int     sep;

    save = save_start = save_string( env);
    while (*save) {
        p = save;
        while (*p && *p != ENV_SEP)
            p++;
        if (p != save)  {                   /* Variable separator   */
            sep = *p;
            *p = EOS;
            set_a_dir( save);
            if (sep == EOS)
                break;
            save = ++p;
        }
        while (*save == ENV_SEP)
            ++save;
    }
    free( save_start);
}

static void set_sys_dirs(
    int     set_cplus_dir       /* Set C++ include-directory too    */
)
/*
 * Set site-specific and system-specific directories to the include directory
 * list.
 */
{
#if SYS_FAMILY == SYS_UNIX
    set_a_dir( "/usr/local/include");
#endif

#ifdef  C_INCLUDE_DIR1
    set_a_dir( C_INCLUDE_DIR1);
#endif
#ifdef  C_INCLUDE_DIR2
    set_a_dir( C_INCLUDE_DIR2);
#endif

#if SYS_FAMILY == SYS_UNIX
#if SYSTEM == SYS_CYGWIN
    if (no_cygwin)                          /* -mno-cygwin          */
        set_a_dir( "/usr/include/mingw");
    else
        set_a_dir( "/usr/include");
#else
    set_a_dir( "/usr/include"); /* Should be placed after C_INCLUDE_DIR?    */
#endif
#endif
}

static void set_a_dir(
    const char *    dirname                 /* The path-name        */
)
/*
 * Append an include directory.
 * This routine is called from the following routines (in this order).
 * 1. do_options() by -I option.
 * 2. do_options() by -isystem option (for GNUC).
 * 3. set_env_dirs() by environment variables.
 * 4. set_sys_dirs() by CPLUS_INCLUDE_DIR?, C_INCLUDE_DIR? and system-
 *    specifics (unless -I- or -nostdinc option is specified).
 * Ignore non-existent directory.
 * Note that this routine should be called only in initializing steps,
 *      because increase of include dirs causes reallocation of incdir[].
 * Note: a trailing PATH-DELIM is appended by norm_path().
 */
{
    char *  norm_name;
    const char **   ip;

    if (incdir == NULL) {               /* Should be initialized    */
        max_inc = INIT_NUM_INCLUDE;
        incdir = (const char **) xmalloc( sizeof (char *) * max_inc);
        incend = &incdir[ 0];
    } else if (incend - incdir >= max_inc) {        /* Buffer full  */
        incdir = (const char **) xrealloc( (void *) incdir
                , sizeof (char *) * max_inc * 2);
        incend = &incdir[ max_inc];
        max_inc *= 2;
    }

    if (dirname == NULL)
        return;                     /* Only to initialize incdir[]  */
    norm_name = norm_dir( dirname, FALSE);
    if (! norm_name)                        /* Non-existent         */
        return;
    for (ip = incdir; ip < incend; ip++) {
        if (str_case_eq( *ip, norm_name)) {
            free( norm_name);               /* Already registered   */
            return;
        }
    }
    /* Register new directory   */
    *incend++ = norm_name;
}

static char *   norm_dir(
    const char *    dirname,        /* Directory path to normalize  */
    int             framework       /* Setting a framework directory*/
)
/*
 * Normalize include directory path.
 * Handle -isysroot option for GCC, including framework directory for SYS_MAC.
 */
{
    char *  norm_name;

    {
        norm_name = norm_path( dirname, NULL, FALSE, FALSE);
                            /* Normalize the pathname to compare    */
    }

    return  norm_name;
}

static char *   norm_path(
    const char *    dir,        /* Include directory (maybe "", never NULL) */
    const char *    fname,
        /* Filename (possibly has directory part, or maybe NULL)    */
    int     inf,    /* If TRUE, output some infs when (mcpp_debug & PATH)   */
    int     hmap            /* "header map" file of Apple-GCC       */
)
/*
 * Normalize the pathname removing redundant components such as
 * "foo/../", "./" and trailing "/.".
 * Append trailing "/" if 'fname' is NULL.
 * Change relative path to absolute path.
 * Dereference a symbolic linked file (or directory) to a real directory/file.
 * Return a malloc'ed buffer, if the directory/file exists.
 * Return NULL, if the specified directory/file does not exist or 'dir' is
 * not a directory or 'fname' is not a regular file.
 * This routine is called from set_a_dir(), init_gcc_macro(), do_once() and
 * open_file().
 */
{
    char *  norm_name;                  /* The path-list converted  */
    char *  start;
    char *  cp1;
    char *  cp2;
    char *  abs_path;
    int     len;                            /* Should not be size_t */
    size_t  start_pos = 0;
    char    slbuf1[ PATHMAX+1];             /* Working buffer       */
#if SYS_FAMILY == SYS_UNIX
    char    slbuf2[ PATHMAX+1]; /* Working buffer for dereferencing */
#endif
#if SYSTEM == SYS_CYGWIN || SYSTEM == SYS_MINGW
    static char *   root_dir;
                /* System's root directory in Windows file system   */
    static size_t   root_dir_len;
#if SYSTEM == SYS_CYGWIN
    static char *   cygdrive = "/cygdrive/";    /* Prefix for drive letter  */
#else
    static char *   mingw_dir;          /* "/mingw" dir in Windows  */
    static size_t   mingw_dir_len;
#endif
#endif
#if HOST_COMPILER == MSC
    struct _stat    st_buf;
#else
    struct stat     st_buf;
#endif

    if (! dir || (*dir && is_full_path( fname)))
        cfatal( "Bug: Wrong argument to norm_path()"        /* _F_  */
                , NULL, 0L, NULL);
    inf = inf && (mcpp_debug & PATH);       /* Output information   */

    strcpy( slbuf1, dir);                   /* Include directory    */
    len = strlen( slbuf1);
    if (fname && len && slbuf1[ len - 1] != PATH_DELIM) {
        slbuf1[ len] = PATH_DELIM;          /* Append PATH_DELIM    */
        slbuf1[ ++len] = EOS;
    } else if (! fname && len && slbuf1[ len - 1] == PATH_DELIM) {
        /* stat() of some systems do not like trailing '/'  */
        slbuf1[ --len] = EOS;
    }
    if (fname)
        strcat( slbuf1, fname);
    if (stat( slbuf1, & st_buf) != 0        /* Non-existent         */
            || (! fname && ! S_ISDIR( st_buf.st_mode))
                /* Not a directory though 'fname' is not specified  */
            || (fname && ! S_ISREG( st_buf.st_mode)))
                /* Not a regular file though 'fname' is specified   */
        return  NULL;
    if (! fname) {
        slbuf1[ len] = PATH_DELIM;          /* Append PATH_DELIM    */
        slbuf1[ ++len] = EOS;
    }
#if SYS_FAMILY == SYS_UNIX
    /* Dereference symbolic linked directory or file, if any    */
    slbuf1[ len] = EOS;     /* Truncate PATH_DELIM and 'fname' part, if any */
    slbuf2[ 0] = EOS;
    if (*dir && ! fname) {      /* Registering include directory    */
        /* Symbolic link check of directories are required  */
        deref_syml( slbuf1, slbuf2, slbuf1);
    } else if (fname) {                             /* Regular file */
        len = strlen( slbuf1);
        strcat( slbuf1, fname);
        deref_syml( slbuf1, slbuf2, slbuf1 + len);
                                /* Symbolic link check of directory */
        if ((len = readlink( slbuf1, slbuf2, PATHMAX)) > 0) {
            /* Dereference symbolic linked file (not directory) */
            *(slbuf2 + len) = EOS;
            cp1 = slbuf1;
            if (slbuf2[ 0] != PATH_DELIM) {     /* Relative path    */
                cp2 = strrchr( slbuf1, PATH_DELIM);
                if (cp2)        /* Append to the source directory   */
                    cp1 = cp2 + 1;
            }
            strcpy( cp1, slbuf2);
        }
    }
    if (inf) {
        if (slbuf2[ 0])
            mcpp_fprintf( DBG, "Dereferenced \"%s%s\" to \"%s\"\n"
                    , dir, fname ? fname : null, slbuf1);
    }
#endif
    len = strlen( slbuf1);
    start = norm_name = xmalloc( len + 1);  /* Need a new buffer    */
    strcpy( norm_name, slbuf1);
#if SYS_FAMILY == SYS_WIN
    bsl2sl( norm_name);
#endif
#if SPECIAL_PATH_DELIM                  /* ':' ?    */
    for (cp1 = norm_name; *cp1 != EOS; cp1++) {
        if (*cp1 == PATH_DELIM)
            *cp1 = '/';
    }
#endif
    cp1 = norm_name;

#if SYSTEM == SYS_CYGWIN
    /* Convert to "/cygdirve/x/dir" style of absolute path-list     */
    if (len >= 8 && (memcmp( cp1, "/usr/bin", 8) == 0
                    || memcmp( cp1, "/usr/lib", 8) == 0)) {
        memmove( cp1, cp1 + 4, len - 4 + 1);    /* Remove "/usr"    */
        len -= 4;
    }
    if (*cp1 == '/' && (len < 10 || memcmp( cp1, cygdrive, 10) != 0)) {
        /* /dir, not /cygdrive/     */
        if (! root_dir_len) {           /* Should be initialized    */
            /* Convert "X:\DIR-list" to "/cygdrive/x/dir-list"      */
            root_dir = xmalloc( strlen( CYGWIN_ROOT_DIRECTORY) + 1);
            strcpy( root_dir, CYGWIN_ROOT_DIRECTORY);
            *(root_dir + 1) = *root_dir;        /* "x:/" to " x/"   */
            cp1 = xmalloc( strlen( cygdrive) + strlen( root_dir));
            strcpy( cp1, cygdrive);
            strcat( cp1, root_dir + 1);
            free( root_dir);
            root_dir = cp1;
            root_dir_len = strlen( root_dir);
        }
        cp1 = xmalloc( root_dir_len + len + 1);
        strcpy( cp1, root_dir);
        strcat( cp1, norm_name);        /* Convert to absolute path */
        free( norm_name);
        norm_name = start = cp1;
        len += root_dir_len;
    }
#endif

#if SYSTEM == SYS_MINGW
    /* Handle the mess of MinGW's path-list */
    /* Convert to "x:/dir" style of absolute path-list  */
    if (*cp1 == PATH_DELIM && isalpha( *(cp1 + 1))
            && *(cp1 + 2) == PATH_DELIM) {          /* /c/, /d/, etc*/
        *cp1 = *(cp1 + 1);
        *(cp1 + 1) = ':';               /* Convert to c:/, d:/, etc */
    } else if (memcmp( cp1, "/mingw", 6) == 0) {
        if (! mingw_dir_len) {          /* Should be initialized    */
            mingw_dir_len = strlen( MINGW_DIRECTORY);
            mingw_dir = xmalloc( mingw_dir_len + 1);
            strcpy( mingw_dir, MINGW_DIRECTORY);
        }
        cp1 = xmalloc( mingw_dir_len + len + 1);
        strcpy( cp1, mingw_dir);
        strcat( cp1, norm_name + 6);    /* Convert to absolute path */
        free( norm_name);
        norm_name = start = cp1;
        len += mingw_dir_len;
    } else if (memcmp( cp1, "/usr", 4) == 0) {
        memmove( cp1, cp1 + 4, len - 4 + 1);    /* Remove "/usr"    */
        len -= 4;
    }
    if (*cp1 == '/') {                  /* /dir or /                */
        if (! root_dir_len) {           /* Should be initialized    */
            root_dir_len = strlen( MSYS_ROOT_DIRECTORY);
            root_dir = xmalloc( root_dir_len + 1);
            strcpy( root_dir, MSYS_ROOT_DIRECTORY);
        }
        cp1 = xmalloc( root_dir_len + len + 1);
        strcpy( cp1, root_dir);
        strcat( cp1, norm_name);        /* Convert to absolute path */
        free( norm_name);
        norm_name = start = cp1;
        len += root_dir_len;
    }
#endif

#if SYS_FAMILY == SYS_WIN
    if (*(cp1 + 1) == ':')
        start = cp1 += 2;               /* Next to the drive letter */
    start_pos = 2;
#endif
    if (len == 1 && *norm_name == '/')              /* Only "/"     */
        return  norm_name;

    if (strncmp( cp1, "./", 2) == 0)    /* Remove beginning "./"    */
        memmove( cp1, cp1 + 2, strlen( cp1 + 2) + 1);       /* +1 for EOS   */
    if (*start != '/') {    /* Relative path to current directory   */
        /* Make absolute path   */
        abs_path = xmalloc( len + strlen( cur_work_dir) + 1);
        cp1 = stpcpy( abs_path, cur_work_dir);
        strcpy( cp1, start);
        free( norm_name);
        norm_name = abs_path;
        start = cp1 = norm_name + start_pos;
    }

    while ((cp1 = strstr( cp1, "/./")) != NULL)
        memmove( cp1, cp1 + 2, strlen( cp1 + 2) + 1);
                                        /* Remove "/." of "/./"     */
    cp1 = start;
    /* Remove redundant "foo/../"   */
    while ((cp1 = strstr( cp1, "/../")) != NULL) {
        *cp1 = EOS;
        if ((cp2 = strrchr( start, '/')) != NULL) {
            if (*(cp1 - 1) != '.') {
                memmove( cp2 + 1, cp1 + 4, strlen( cp1 + 4) + 1);
                                        /* Remove "foo/../"         */
                cp1 = cp2;
            } else {                                /* Impossible   */
                break;
            }
        } else {                                    /* Impossible   */
            break;
        }
    }

#if SPECIAL_PATH_DELIM
    for (cp1 = start; *cp1 != EOS; cp1++) {
        if (*cp1 == '/')
            *cp1 = PATH_DELIM;
    }
#endif
    if (inf) {
        char    debug_buf[ PATHMAX+1];
        strcpy( debug_buf, dir);
        strcat( debug_buf, fname ? fname : null);
#if SYS_FAMILY == SYS_WIN
        bsl2sl( debug_buf);
#endif
        if (! str_eq( debug_buf, norm_name))
            mcpp_fprintf( DBG, "Normalized the path \"%s\" to \"%s\"\n"
                    , debug_buf, norm_name);
    }

    return  norm_name;
}

#if SYS_FAMILY == SYS_UNIX

static void     deref_syml(
    char *      slbuf1,                     /* Original path-list   */
    char *      slbuf2,                     /* Working buffer       */
    char *      chk_start                   /* Pointer into slbuf1  */
)
/* Dereference symbolic linked directory    */
{
    char *      cp2;
    int         len;                /* Should be int, not size_t    */

    while ((chk_start = strchr( chk_start, PATH_DELIM)) != NULL) {
        *chk_start = EOS;
        if ((len = readlink( slbuf1, slbuf2, PATHMAX)) > 0) {
            /* Dereference symbolic linked directory    */
            cp2 = strrchr( slbuf1, PATH_DELIM); /* Previous delimiter       */
            *chk_start = PATH_DELIM;
            strcpy( slbuf2 + len, chk_start);
            if (slbuf2[ 0] == PATH_DELIM) {     /* Absolute path    */
                strcpy( slbuf1, slbuf2);
                chk_start = slbuf1 + len + 1;
            } else {
                if (cp2)
                    chk_start = cp2 + 1;
                else
                    chk_start = slbuf1;
                strcpy( chk_start, slbuf2);     /* Rewrite the path */
                chk_start += len;
            }
        } else {
            *chk_start++ = PATH_DELIM;
        }
    }
}
#endif

static void     def_macros( void)
/*
 * Define macros specified by -D option.
 * This routine should be called before undef_macros().
 */
{
    int         i;

    for (i = 0; i < def_cnt; i++)
        def_a_macro( 'D', def_list[ i]);
}

static void     undef_macros( void)
/*
 * Undefine macros specified by -U option.
 * This routine should be called after init_predefine().
 */
{
    char *      name;
    int         i;

    for (i = 0; i < undef_cnt; i++) {
        name = undef_list[ i];
        if (look_id( name) != NULL)
            undefine( name);
        else if (warn_level & 8)
            mcpp_fprintf( ERR, "\"%s\" wasn't defined\n", name);
    }
}

void    put_depend(
    const char *    filename
)
/*
 * Append a header name to the source file dependency line.
 */
{
#define MAX_OUT_LEN     76      /* Maximum length of output line    */
#define MKDEP_INITLEN   (MKDEP_INIT * 0x100)
#define MKDEP_MAX       (MKDEP_INIT * 0x10)
#define MKDEP_MAXLEN    (MKDEP_INITLEN * 0x10)

    static char *   output = NULL;          /* File names           */
    static size_t * pos = NULL;             /* Offset to filenames  */
    static int      pos_num;                /* Index of pos[]       */
    static char *   out_p;                  /* Pointer to output[]  */
    static size_t   mkdep_len;              /* Size of output[]     */
    static size_t   pos_max;                /* Size of pos[]        */
    static FILE *   fp;         /* Path to output dependency line   */
    static size_t   llen;       /* Length of current physical output line   */
    size_t *        pos_p;                  /* Index into pos[]     */
    size_t          fnamlen;                /* Length of filename   */

    if (fp == NULL) {   /* Main source file.  Have to initialize.   */
        if (output != NULL) {
            free( output);
            free( pos);
        }
        output = xmalloc( mkdep_len = MKDEP_INITLEN);
        pos = (size_t *) xmalloc( (pos_max = MKDEP_INIT) * sizeof (size_t));
        out_p = md_init( filename, output);
        fp = mkdep_fp;
        llen = strlen( output);
        pos_num = 0;            /* Initialize for MCPP_LIB build    */
    } else if (filename == NULL) {              /* End of input     */
        out_p = stpcpy( out_p, "\n\n");
        if (mkdep & MD_PHONY) {
            /* Output the phony target line for each recorded header files. */
            char *  cp;
            int     c;

            if (strlen( output) * 2 + (pos_num * 2) >= MKDEP_MAXLEN) {
                cerror( "Too long dependency line"          /* _E_  */
                        , NULL, 0L, NULL);
                if (fp == fp_out)
                    mcpp_fputs( output, OUT);
                else
                    fputs( output, fp);
                return;
            } else if (strlen( output) * 2 + (pos_num * 2) >= mkdep_len) {
                /* Enlarge the buffer   */
                size_t  len = out_p - output;
                output = xrealloc( output, mkdep_len *= 2);
                out_p = output + len;
            }
            pos_num--;
            for (pos_p = &pos[ 0]; pos_p <= &pos[ pos_num]; pos_p++) {
                if (pos_p == &pos[ pos_num]) {      /* End of output    */
                    for (cp = output + *pos_p; *cp != '\n'; cp++)
                        ;
                    c = '\n';                       /* Append newline   */
                } else {
                    cp = output + *(pos_p + 1) - 1;
                    while( *cp == ' ' || *cp == '\\' || *cp == '\n')
                        cp--;               /* Remove trailing spaces   */
                    c = *(++cp);
                }
                *cp = EOS;
                out_p = stpcpy( out_p, output + *pos_p);
                out_p = stpcpy( out_p, ":\n\n");
                *cp = c;
            }
        }
        if (fp == fp_out) { /* To the same path with normal preprocessing   */
            mcpp_fputs( output, OUT);
        } else {        /* To the file specified by -MF, -MD, -MMD options  */
            fputs( output, fp);
            fclose( fp);
        }
        fp = NULL;      /* Clear for the next call in MCPP_LIB build        */
        return;
    }

    fnamlen = strlen( filename);
    /* Check the recorded filename  */
    for (pos_p = pos; pos_p < &pos[ pos_num]; pos_p++) {
        if (memcmp( output + *pos_p, filename, fnamlen) == 0)
            return;                 /* Already recorded filename    */
    }
    /* Any new header.  Append its name to output.  */
    if (llen + fnamlen > MAX_OUT_LEN) {         /* Line is long     */
        out_p = stpcpy( out_p, " \\\n ");       /* Fold it          */
        llen = 1;
    }
    llen += fnamlen + 1;
    if (pos_num >= MKDEP_MAX
            || out_p + fnamlen + 1 >= output + MKDEP_MAXLEN)
        cfatal( "Too long dependency line: %s", output, 0L, NULL);
    /* Need to enlarge the buffer   */
    if (pos_num >= pos_max) {
        pos = (size_t *) xrealloc( (char *) pos
                , (pos_max *= 2) * sizeof (size_t *));
    }
    if (output + mkdep_len <= out_p + fnamlen + 1) {
        size_t  len = out_p - output;
        output = xrealloc( output, mkdep_len *= 2);
        out_p = output + len;
    }
    *out_p++ = ' ';
    pos[ pos_num++] = out_p - output;       /* Remember the offset  */
            /* Don't use pointer, since 'output' may be reallocated later.  */
    out_p = stpcpy( out_p, filename);
}

static char *   md_init(
    const char *    filename,   /* The source file name             */
    char *  output              /* Output to dependency file        */
)
/*
 * Initialize output file and target.
 */
{
    char    prefix[ PATHMAX];
    char *  cp = NULL;
    size_t  len;
    char *  out_p;
    const char *    target = filename;
    const char *    cp0;

    if (! mkdep_target || ! mkdep_fp) {         /* Make target name */
#ifdef  PATH_DELIM
        if ((cp0 = strrchr( target, PATH_DELIM)) != NULL)
            target = cp0 + 1;
#endif
        if ((cp0 = strrchr( target, '.')) == NULL)
            len = strlen( target);
        else
            len = (size_t) (cp0 - target);
        memcpy( prefix, target, len);
        cp = prefix + len;
        *cp++ = '.';
    }

    if (! mkdep_fp) {   /* Unless already opened by -MF, -MD, -MMD options  */
        if (mkdep & MD_FILE) {
            strcpy( cp, "d");
            mkdep_fp = mcpp_fopen(prefix, "w");
        } else {
            mkdep_fp = fp_out;  /* Output dependency line to normal output  */
            no_output++;                /* Without normal output    */
        }
    }

    if (mkdep_target) {         /* -MT or -MQ option is specified   */
        if (mkdep & MD_QUOTE) {         /* 'Quote' $, \t and space  */
            out_p = md_quote( output);
        } else {
            out_p = stpcpy( output, mkdep_target);
        }
    } else {
        strcpy( cp, OBJEXT);
        out_p = stpcpy( output, prefix);
    }

    *out_p++ = ':';
    *out_p = EOS;
    return  out_p;
}

static char *   md_quote(
    char *  output
)
/*
 * 'Quote' $, tab and space.
 * This function was written referring to GCC V.3.2 source.
 */
{
    char *  p;
    char *  q;

    for (p = mkdep_target; *p; p++, output++) {
        switch (*p) {
        case ' ':
        case '\t':
            /* GNU-make treats backslash-space sequence peculiarly  */
            for (q = p - 1; mkdep_target <= q && *q == '\\'; q--)
                *output++ = '\\';
            *output++ = '\\';
            break;
        case '$':
            *output++ = '$';
            break;
        default:
            break;
        }
        *output = *p;
    }
    *output = EOS;
    return  output;
}

static const char *     toolong_fname =
        "Too long header name \"%s%.0ld%s\"";               /* _F_  */
static const char *     excess_token =
        "Excessive token sequence \"%s\"";          /* _E_, _W1_    */

int     do_include(
    int     next        /* TRUE if the directive is #include_next   */
)
/*
 * Process the #include line.
 * There are three variations:
 *      #include "file"         search somewhere relative to the
 *                              current (or source) directory, if not
 *                              found, treat as #include <file>.
 *      #include <file>         Search in an implementation-dependent
 *                              list of places.
 *      #include macro-call     Expand the macro call, it must be one of
 *                              "file" or <file>, process as such.
 * On success : return TRUE;
 * On failure of syntax : return FALSE;
 * On failure of file opening : return FALSE.
 * do_include() always absorbs the line (including the <newline>).
 */
{
    const char * const  no_name = "No header name";         /* _E_  */
    char    header[ PATHMAX + 16];
    int     token_type;
    char *  fname;
    char *  filename;
    int     delim;                          /* " or <, >            */

    if ((delim = skip_ws()) == '\n') {      /* No argument          */
        cerror( no_name, NULL, 0L, NULL);
        return  FALSE;
    }
    fname = infile->bptr - 1;       /* Current token for diagnosis  */

    if ((char_type[ delim] & LET)) {    /* Maybe macro  */
        int     c;
        char    *hp;

        hp = header;
        *hp = EOS;
        c = delim;
        while (get_unexpandable( c, FALSE) != NO_TOKEN) {
                                /* Expand any macros in the line    */
            if (header + PATHMAX < hp + (int) (workp - work_buf))
                cfatal( toolong_fname, header, 0L, work_buf);
            hp = stpcpy( hp, work_buf);
            while (char_type[ c = get_ch()] & HSP)
                *hp++ = c;
        }
        *hp = EOS;                          /* Ensure to terminate  */
        if (macro_line == MACRO_ERROR)      /* Unterminated macro   */
            return  FALSE;                  /*   already diagnosed. */
        unget_string( header, NULL);        /* To re-read           */
        delim = skip_ws();
        if (delim == '\n') {
            cerror( no_name, NULL, 0L, NULL);       /* Expanded to  */
            return  FALSE;                          /*   0 token.   */
        }
    }

    token_type = scan_token( delim, (workp = work_buf, &workp)
            , work_buf + PATHMAX);
    if (token_type == STR)                  /* String literal form  */
        goto  found_name;
    else if (token_type == OPE && openum == OP_LT)          /* '<'  */
        workp = scan_quote( delim, work_buf, work_buf + PATHMAX, TRUE);
                                        /* Re-construct or diagnose */
    else                                    /* Any other token in-  */
        goto  not_header;                   /*   cluding <=, <<, <% */

    if (workp == NULL)                      /* Missing closing '>'  */
        goto  syntax_error;

found_name:
    *--workp = EOS;                         /* Remove the closing and   */
    fname = save_string( &work_buf[ 1]);    /*  the starting delimiter. */

    if (skip_ws() != '\n') {
        cerror( excess_token, infile->bptr-1, 0L, NULL);
        skip_nl();
        goto  error;
    }

#if SYS_FAMILY == SYS_WIN
    bsl2sl( fname);
#endif
    filename = fname;
#if NO_DIR                              /* Unofficial feature           */
    if (no_dir) {                       /* Strip directory components   */
        char    src_dir[ PATHMAX] = { EOS, };
        if (has_directory( fname, src_dir))
            filename = fname + strlen( src_dir);
        delim = '"';    /* Even a system header is handled as a local one   */
    }
#endif
    if (open_include( filename, (delim == '"'), next)) {
        /* 'fname' should not be free()ed, it is used as file->         */
        /*      real_fname and has been registered into fnamelist[]     */
        return  TRUE;
    }

    cerror( "Can't open include file \"%s\"", filename, 0L, NULL);  /* _E_  */
error:
    free( fname);
    return  FALSE;

not_header:
    cerror( "Not a header name \"%s\"", fname, 0L, NULL);   /* _E_  */
syntax_error:
    skip_nl();
    return  FALSE;
}

static int  open_include(
    char *  filename,               /* File name to include         */
    int     searchlocal,            /* TRUE if #include "file"      */
    int     next                    /* TRUE if #include_next        */
)
/*
 * Open an include file.  This routine is only called from do_include() above.
 * It searches the list of directories via search_dir() and opens the file
 * via open_file(), linking it into the list of active files.
 * Returns TRUE if the file was opened, FALSE if it fails.
 */
{
    char    src_dir[ PATHMAX] = { EOS, };   /* Directory part of includer   */
    int     full_path;              /* Filename is full-path-list   */
    int     has_dir = FALSE;        /* Includer has directory part  */
    int     has_dir_src = FALSE;
    int     has_dir_fname = FALSE;

    full_path = is_full_path( filename);

    if (!full_path && searchlocal && (search_rule & SOURCE)) {
        has_dir_src  = has_directory( infile->src_dir, src_dir);
        has_dir_fname = has_directory( infile->real_fname
                , src_dir + strlen( src_dir));
        /* Get directory part of the parent file of the file to include.*/
        /* Note that infile->dirp of main input file is set to "" and   */
        /* remains the same even if -include options are processed.     */
        has_dir = has_dir_src || has_dir_fname
                || (**(infile->dirp) != EOS);
    }
    if (mcpp_debug & PATH)
        mcpp_fprintf( DBG, "filename: %s\n", filename);

    if ((searchlocal && ((search_rule & CURRENT) || !has_dir)) || full_path) {
        /*
         * Look in local directory first.
         * Try to open filename relative to the "current directory".
         */
        if (open_file( &null, NULL, filename, searchlocal && !full_path
                , FALSE, FALSE))
            return  TRUE;
        if (full_path)
            return  FALSE;
    }

    if (searchlocal && (search_rule & SOURCE) && has_dir) {
        /*
         * Look in local directory of source file.
         * Try to open filename relative to the "source directory".
         */
        if (open_file( infile->dirp, src_dir, filename, TRUE, FALSE, FALSE))
            return  TRUE;
    }

    /* Search the include directories   */
    if (search_dir( filename, searchlocal, next))
        return  TRUE;

    return  FALSE;
}

static int  has_directory(
    const char *    source,         /* Filename to examine          */
    char *  directory               /* Put directory stuff here     */
)
/*
 * If a directory is found in the 'source' filename string (i.e. "includer"),
 * the directory part of the string is copied to 'directory' and
 * has_directory() returns TRUE.
 * Else, nothing is copied and it returns FALSE.
 */
{
    const char *    sp;
    size_t  len;

    if (! source)
        return  FALSE;
    if ((sp = strrchr( source, PATH_DELIM)) == NULL) {
        return  FALSE;
    } else {
        len = (size_t)(sp - source) + 1;    /* With path-delimiter  */
        memcpy( directory, source, len);
        directory[ len] = EOS;
        return  TRUE;
    }
}

static int  is_full_path(
    const char *    path
)
/*
 * Check whether the path is a full (absolute) path list or not.
 */
{
    if (! path)
        return  FALSE;
#if SYS_FAMILY == SYS_UNIX
    if (path[0] == PATH_DELIM)
#elif   SYS_FAMILY == SYS_WIN
    if ((path[1] == ':' && path[2] == PATH_DELIM)   /* "C:/path"    */
            || path[0] == PATH_DELIM)       /* Root dir of current drive    */
#elif   1
/* For other systems you should write code here.    */
    if (path[0] == PATH_DELIM)
#endif
        return  TRUE;
    else
        return  FALSE;
}

static int  search_dir(
    char *  filename,               /* File name to include         */
    int     searchlocal,            /* #include "header.h"          */
    int     next                    /* TRUE if #include_next        */
)
/*
 * Look in any directories specified by -I command line arguments,
 * specified by environment variable, then in the builtin search list.
 */
{
    const char **   incptr;                 /* -> inlcude directory */

    incptr = incdir;

    for ( ; incptr < incend; incptr++) {
        if (strlen( *incptr) + strlen( filename) >= PATHMAX)
            cfatal( toolong_fname, *incptr, 0L, filename);  /* _F_  */
        if (open_file( incptr, NULL, filename, FALSE, FALSE, FALSE))
            /* Now infile has been renewed  */
            return  TRUE;
    }

    return  FALSE;
}

static int  open_file(
    const char **   dirp,           /* Pointer to include directory */
    const char *    src_dir,        /* Source directory of includer */
    const char *    filename,       /* Filename (possibly has directory)    */
    int         local,                      /* #include "file"      */
    int         include_opt,        /* Specified by -include option */
    int         sys_frame           /* System framework header (for SYS_MAC)*/
)
/*
 * Open a file, add it to the linked list of open files, close the includer
 * if nessesary and truncate the includer's buffer.
 * This is called from open_include() and at_start().
 */
{
    char        dir_fname[ PATHMAX] = { EOS, };
#if HOST_COMPILER == BORLANDC
    /* Borland's fopen() does not set errno.    */
    static int  max_open = FOPEN_MAX - 5;
#else
    static int  max_open;
#endif
    int         len;
    FILEINFO *  file = infile;
    FILE *      fp;
    char *      fullname;
    const char *    fname;

    errno = 0;      /* Clear errno possibly set by path searching   */
    {
        if (mcpp_debug & PATH)
            mcpp_fprintf( DBG, "Searching %s%s%s\n", *dirp
                    , src_dir ? src_dir : null, filename);
    }
    /* src_dir is usually NULL.  This is specified to   */
    /* search the source directory of the includer.     */
    if (src_dir && *src_dir != EOS) {
        strcpy( dir_fname, src_dir);
        strcat( dir_fname, filename);
        fname = dir_fname;
    } else {
        fname = filename;
    }

    fullname = norm_path( *dirp, fname, TRUE, FALSE);
                                    /* Convert to absolute path     */
    if (! fullname)                 /* Non-existent or directory    */
        return  FALSE;
    if (included( fullname))        /* Once included    */
        goto  true;

    if ((max_open != 0 && max_open <= include_nest)
                            /* Exceed the known limit of open files */
            || ((fp = mcpp_fopen( fullname, "r")) == NULL && errno == EMFILE)) {
                            /* Reached the limit for the first time */
        if (mcpp_debug & PATH) {
#if HOST_COMPILER == BORLANDC
            if (include_nest == FOPEN_MAX - 5)
#else
            if (max_open == 0)
#endif
                mcpp_fprintf( DBG,
    "#include nest reached at the maximum of system: %d, returned errno: %d\n"
                    , include_nest, errno);
        }
        /*
         * Table of open files is full.
         * Remember the file position and close the includer.
         * The state will be restored by get_line() on end of the included.
         */
        file->pos = ftell( file->fp);
        fclose( file->fp);
        /* In case of failure, re-open the includer */
        if ((fp = mcpp_fopen( fullname, "r")) == NULL) {
            file->fp = mcpp_fopen( cur_fullname, "r");
            fseek( file->fp, file->pos, SEEK_SET);
            goto  false;
        }
        if (max_open == 0)      /* Remember the limit of the system */
            max_open = include_nest;
    } else if (fp == NULL)                  /* No read permission   */
        goto  false;
    /* Truncate buffer of the includer to save memory   */
    len = (int) (file->bptr - file->buffer);
    if (len) {
        file->buffer = xrealloc( file->buffer, len + 1);
        file->bptr = file->buffer + len;
    }

    if (! include_opt)
        sharp( NULL, 0);    /* Print includer's line num and fname  */
    add_file( fp, src_dir, filename, fullname, include_opt);
    /* Add file-info to the linked list.  'infile' has been just renewed    */
    /*
     * Remember the directory for #include_next.
     * Note: inc_dirp is restored to the parent includer's directory
     *   by get_ch() when the current includer is finished.
     */
    infile->dirp = inc_dirp = dirp;
#if 0   /* This part is only for debugging  */
    chk_dirp( dirp);
#endif
    cur_fullname = fullname;

    if (! include_opt) {     /* Do not sharp() on -include   */
        src_line = 1;                   /* Working on line 1 now    */
        sharp( NULL, 1);    /* Print out the included file name     */
    }
    src_line = 0;                       /* To read the first line   */

    if (mkdep && ((mkdep & MD_SYSHEADER) || ! infile->sys_header))
        put_depend( fullname);          /* Output dependency line   */

true:
    return  TRUE;
false:
    free( fullname);
    return  FALSE;
}

void    add_file(
    FILE *      fp,                         /* Open file pointer    */
    const char *    src_dir,                /* Directory of source  */
    const char *    filename,               /* Name of the file     */
    const char *    fullname,               /* Full path list       */
    int         include_opt         /* File specified by -include option    */
)
/*
 * Initialize tables for this open file.  This is called from open_file()
 * (for #include files), and from the entry to MCPP to open the main input
 * file.  It calls a common routine get_file() to build the FILEINFO
 * structure which is used to read characters.
 */
{
    FILEINFO *      file;
    const char *    too_many_include_nest =
            "More than %.0s%ld nesting of #include";    /* _F_ _W4_ */

    //
    // When encoding is UTF-8, skip BOM if present.
    //
    if(fp != NULL && ftell(fp) == 0)
    {
        const unsigned char UTF8_BOM[3] = {0xEF, 0xBB, 0xBF};
        unsigned char FILE_HEAD[3] = {0, 0, 0};
        int i;
        for(i = 0; i < 3; ++i)
        {
            FILE_HEAD[i] = getc(fp);
            if(FILE_HEAD[i] != UTF8_BOM[i])
            {
                if(FILE_HEAD[i] == (unsigned char)EOF)
                {
                    i--;
                }
                for(; i >= 0; --i)
                {
                    ungetc(FILE_HEAD[i], fp);
                }
                break;
            }
        }
    }

    filename = set_fname( filename);    /* Search or append to fnamelist[]  */
    fullname = set_fname( fullname);    /* Search or append to fnamelist[]  */
    file = get_file( filename, src_dir, fullname, (size_t) NBUFF, include_opt);
                                        /* file == infile           */
    file->fp = fp;                      /* Better remember FILE *   */
    cur_fname = filename;

    if (include_nest >= INCLUDE_NEST)   /* Probably recursive #include      */
        cfatal( too_many_include_nest, NULL, (long) INCLUDE_NEST, NULL);
    if ((warn_level & 4)
            && include_nest == std_limits.inc_nest + 1)
        cwarn( too_many_include_nest, NULL, (long) std_limits.inc_nest, NULL);
    include_nest++;
}

static const char *     set_fname(
    const char *    filename
)
/*
 * Register the source filename to fnamelist[].
 * Search fnamelist[] for filename or append filename to fnamelist[].
 * Returns the pointer.
 * file->real_fname and file->full_fname points into fnamelist[].
 */
{
    INC_LIST *  fnamep;
    size_t      fnamelen;

    if (fnamelist == NULL) {            /* Should be initialized    */
        max_fnamelist = INIT_NUM_FNAMELIST;
        fnamelist = (INC_LIST *) xmalloc( sizeof (INC_LIST) * max_fnamelist);
        fname_end = &fnamelist[ 0];
    } else if (fname_end - fnamelist >= max_fnamelist) {
                                /* Buffer full: double the elements */
        fnamelist = (INC_LIST *) xrealloc( (void *) fnamelist
                , sizeof (INC_LIST) * max_fnamelist * 2);
        fname_end = &fnamelist[ max_fnamelist];
        max_fnamelist *= 2;
    }

    /* Register the filename in fnamelist[] */
    fnamelen = strlen( filename);
    for (fnamep = fnamelist; fnamep < fname_end; fnamep++) {
        if (fnamep->len == fnamelen && str_case_eq( fnamep->name, filename))
            return  filename;           /* Already registered       */
    }
    fname_end->name = xmalloc( fnamelen + 1);
    filename = strcpy( fname_end->name, filename);
                                /* Global pointer for get_file()    */
    fname_end->len = fnamelen;
    fname_end++;

    return  filename;
}

#if 0   /* This part is only for debugging  */
static int  chk_dirp(
    const char **   dirp
)
/*
 * Check the validity of include directory specified for open_file().
 * Called only from open_file().
 */
{
    const char **   ip;

    if (dirp == &null)
        return  TRUE;

    for (ip = incdir; ip < incend; ip++)
        if (dirp == ip)
            break;
    if (ip == incend) {
#if COMPILER == MSC
        FILEINFO *  pfile = infile->parent;
        if (pfile) {
            while ((pfile = pfile->parent) != NULL) {
                /* Search each parent includer's directory  */
                if (dirp == pfile->dirp)
                    break;
            }
        }
        if (! pfile)
#endif
#if COMPILER == GNUC
        const char **   qdir;
        for (qdir = quote_dir; qdir < quote_dir_end; qdir++) {
            if (dirp == qdir)
                break;
        }
        if (qdir == quote_dir_end)
#endif
        {
            cfatal( "Bug: *dirp:%s is invalid", *dirp, 0L, NULL);
            return  FALSE;
        }
    }
    return  TRUE;
}
#endif

FILEINFO*       sh_file;
int             sh_line;

void    sharp(
    FILEINFO *  sharp_file,
    int         flag        /* Flag to append to the line for GCC   */
)
/*
 * Output a line number line.
 * 'file' is 'sharp_file' if specified,
 * else (i.e. 'sharp_file' is NULL) 'infile'.
 */
{
    FILEINFO *  file;
    int         line;

    file = sharp_file ? sharp_file : infile;
    if (! file)
        return;
    while (! file->fp)
        file = file->parent;
    line = sharp_file ? sharp_file->line : src_line;
    if (no_output || file == NULL
            || (file == sh_file && line == sh_line))
        goto  sharp_exit;
    sh_file = file;
    sh_line = line;
    if (keep_comments)
        mcpp_fputc( '\n', OUT);         /* Ensure to be on line top */
    if (std_line_prefix)
        mcpp_fprintf( OUT, "#line %ld", line);
    else
        mcpp_fprintf( OUT, "%s%ld", LINE_PREFIX, line);
    cur_file( file, sharp_file, flag);
    mcpp_fputc( '\n', OUT);
sharp_exit:
    wrong_line = FALSE;
}

static void cur_file(
    FILEINFO *  file,                   /* infile or sharp_file     */
    FILEINFO *  sharp_file,             /* The 'file' or NULL       */
    int         flag                    /* Flag to append for GCC   */
)
/*
 * Output current source file name and line number.
 * Called only from sharp() above.
 */
{
    const char *    name;

    if (mcpp_debug & MACRO_CALL) {  /* In macro notification mode   */
        if (sharp_file)                         /* Main input file  */
            name = file->filename;
        else                /* Output full-path-list, normalized    */
            name = cur_fullname;
    } else {                /* Usually, the path not "normalized"   */
        if (sharp_file) {                       /* Main input file  */
            name = file->filename;
        } else if (str_eq( file->filename, file->real_fname)) {
            sprintf( work_buf, "%s%s", *(file->dirp), cur_fname);
            name = work_buf;
        } else {            /* Changed by '#line fname' directive   */
            name = file->filename;
        }
    }
    if (sharp_filename == NULL || ! str_eq( name, sharp_filename)) {
        if (sharp_filename != NULL)
            free( sharp_filename);
        sharp_filename = save_string( name);
    }
    mcpp_fprintf( OUT, " \"%s\"", name);
}

#if SYS_FAMILY == SYS_WIN

static char *   bsl2sl(
    char * filename
)
/*
 * Convert '\\' in the path-list to '/'.
 */
{
    static int  diagnosed = FALSE;
    char *  cp;

    cp = filename;

    while (*cp) {
        if (*cp == '\\') {
            *cp++ = PATH_DELIM;
            if (!diagnosed && (warn_level & 2) && (warn_level != -1)) {
                            /* Backslash in source program          */
                cwarn( "Converted \\ to %s", "/", 0L, NULL);        /* _W2_ */
                    diagnosed = TRUE;       /* Diagnose only once   */
            }
        } else {
            cp++;
        }
    }

    return  filename;
}

#endif  /* SYS_FAMILY == SYS_WIN    */

static const char * const   unknown_arg =
        "Unknown argument \"%s\"";      /*_W1_*/
static const char * const   not_ident =
        "Not an identifier \"%s\"";     /*_W1_*/

static int  is_junk( void)
/*
 * Check the trailing junk in a directive line.
 * This routine is never called in OLD_PREP mode.
 */
{
    int     c;

    c = skip_ws();
    unget_ch();
    if (c != '\n') {                        /* Trailing junk        */
        if (warn_level & 1)
            cwarn( unknown_arg, infile->bptr, 0L, NULL);
        return TRUE;
    } else {
        return FALSE;
    }
}

#define PUSH    1
#define POP    -1

#define __SETLOCALE     1       /* #pragma __setlocale( "encoding") */
#define SETLOCALE       2       /* #pragma setlocale( "encoding")   */

void    do_pragma( void)
/*
 * Process the #pragma lines.
 * 1. Process the sub-directive for MCPP.
 * 2. Pass the line to the compiler-proper.
 *      #pragma MCPP put_defines, #pragma MCPP preprocess,
 *      #pragma MCPP preprocessed and #pragma once are, however, not put
 *      out so as not to duplicate output when re-preprocessed.
 * When EXPAND_PRAGMA == TRUE and (__STDC_VERSION__ >= 199901L or
 * __cplusplus >= 199901L), the line is subject to macro expansion unless
 * the next to 'pragma' token is one of 'STDC', 'GCC' or 'MCPP'.
 */
{
    int         c;
    int         warn = FALSE;               /* Necessity of warning */
    int         token_type;
    char *      bp;                         /* Pointer to argument  */
    char *      tp;
    FILEINFO *  file;

    wrong_line = TRUE;                      /* In case of error     */
    c = skip_ws();
    bp = infile->bptr - 1;  /* Remember token to pass to compiler   */
    if (c == '\n') {
        if (warn_level & 1)
            cwarn( "No sub-directive", NULL, 0L, NULL);     /* _W1_ */
        unget_ch();
        return;
    }
    token_type = scan_token( c, (tp = work_buf, &tp), work_end);
#if EXPAND_PRAGMA
    if (stdc3 && token_type == NAM
            && !str_eq( identifier, "STDC") && !str_eq( identifier, "MCPP")) {
        DEFBUF *        defp;
        char *          mp;
        char *          mp_end;
        LINE_COL        line_col = { 0L, 0};

        bp = mp = xmalloc( (size_t)(NMACWORK + IDMAX));
                                    /* Buffer for macro expansion   */
        mp_end = mp + NMACWORK;
        tp = stpcpy( mp, identifier);
        do {                /* Expand all the macros in the line    */
            int     has_pragma;
            if (token_type == NAM && (defp = is_macro( &tp)) != NULL) {
                tp = expand_macro( defp, bp, mp_end, line_col, & has_pragma);
                if (has_pragma)
                cerror( "_Pragma operator found in #pragma line"    /* _E_  */
                            , NULL, 0L, NULL);
                if (! stdc3 && (warn_level & 2))
                    cwarn(
                "\"%s\" is macro expanded in other than C99 mode"   /* _W2_ */
                            , identifier, 0L, NULL);
            }
            token_type = scan_token( c = get_ch(), (bp = tp, &tp), mp_end);
        } while (c != '\n');
        unget_string( mp, NULL);                    /* To re-read   */
        free( mp);
        c = skip_ws();
        bp = infile->bptr - 1;
        token_type = scan_token( c, (tp = work_buf, &tp), work_end);
    }
#endif
    if (token_type != NAM) {
        if (warn_level & 1)
            cwarn( not_ident, work_buf, 0L, NULL);
        goto  skip_nl;
    } else if (str_eq( identifier, "once")) {   /* #pragma once     */
       if (! is_junk()) {
            file = infile;
            while (file->fp == NULL)
                file = file->parent;
            do_once( file->full_fname);
            goto  skip_nl;
        }
    } else if (str_eq( identifier, "MCPP")) {
        if (scan_token( skip_ws(), (tp = work_buf, &tp), work_end) != NAM) {
            if (warn_level & 1)
                cwarn( not_ident, work_buf, 0L, NULL);
        }
        if (str_eq( identifier, "put_defines")) {
            if (! is_junk())
                dump_def( TRUE, FALSE); /* #pragma MCPP put_defines */
        } else if (str_eq( identifier, "preprocess")) {
            if (! is_junk())            /* #pragma MCPP preprocess  */
                mcpp_fputs( "#pragma MCPP preprocessed\n", OUT);
                    /* Just putout the directive    */
        } else if (str_eq( identifier, "preprocessed")) {
            if (! is_junk()) {          /* #pragma MCPP preprocessed*/
                skip_nl();
                do_preprocessed();
                return;
            }
        } else if (str_eq( identifier, "warning")) {
                                        /* #pragma MCPP warning     */
            cwarn( infile->buffer, NULL, 0L, NULL);
        } else if (str_eq( identifier, "push_macro")) {
            push_or_pop( PUSH);         /* #pragma MCPP push_macro  */
        } else if (str_eq( identifier, "pop_macro")) {
            push_or_pop( POP);          /* #pragma MCPP pop_macro   */
        } else if (str_eq( identifier, "debug")) {
            do_debug( TRUE);            /* #pragma MCPP debug       */
        } else if (str_eq( identifier, "end_debug")) {
            do_debug( FALSE);           /* #pragma MCPP end_debug   */
        } else {
            warn = TRUE;
        }
        if (warn && (warn_level & 1))
            cwarn( unknown_arg, identifier, 0L, NULL);
        goto  skip_nl;                  /* Do not putout the line   */

    }

    if (warn) {
        if (warn_level & 1)
            cwarn( unknown_arg, identifier, 0L, NULL);
        goto  skip_nl;                  /* Do not putout the line   */
    }

    sharp( NULL, 0);    /* Synchronize line number before output    */
    if (! no_output) {
        mcpp_fputs( "#pragma ", OUT);
        mcpp_fputs( bp, OUT);           /* Line is put out          */
    }
skip_nl: /* Don't use skip_nl() which skips to the newline in source file */
    while (get_ch() != '\n')
        ;
}

static void do_once(
    const char *    fullname        /* Full-path-list of the header */
)
/*
 * Process #pragma once so as not to re-include the file later.
 * This directive has been imported from GCC V.1.* / cpp as an extension.
 */
{
    if (once_list == NULL) {                /* Should initialize    */
        max_once = INIT_NUM_ONCE;
        once_list = (INC_LIST *) xmalloc( sizeof (INC_LIST) * max_once);
        once_end = &once_list[ 0];
    } else if (once_end - once_list >= max_once) {
                                            /* Double the elements  */
        once_list = (INC_LIST *) xrealloc( (void *) once_list
                , sizeof (INC_LIST) * max_once * 2);
        once_end = &once_list[ max_once];
        max_once *= 2;
    }
    once_end->name = (char*)fullname;
    once_end->len = strlen( fullname);
    once_end++;
}

static int  included(
    const char *    fullname
)
/*
 * Has the file been once included ?
 * This routine is only called from open_file().
 */
{
    INC_LIST *  inc;
    size_t      fnamelen;

    if (once_list == NULL)              /* No once file registered  */
        return  FALSE;
    fnamelen = strlen( fullname);
    for (inc = once_list; inc < once_end; inc++) {
        if (inc->len == fnamelen && str_case_eq( inc->name, fullname)) {
            /* Already included */
            if (mcpp_debug & PATH)
                mcpp_fprintf( DBG, "Once included \"%s\"\n", fullname);
            return  TRUE;
        }
    }
    return  FALSE;                          /* Not yet included     */
}

static void push_or_pop(
    int     direction
)
/* Process #pragma MCPP push_macro( "MACRO"),
 * #pragma MCPP pop_macro( "MACRO") for other compilers than Visual C,
 * and #pragma push_macro( "MACRO"), #pragma pop_macro( "MACRO") for Visual C.
 * Note:1. "push" count is set in defp->push.
 *      2. pushed definitions are inserted immediatly after the current
 *          definition of the same name.
 *      3. the definitions of a same name macro can be pushed multiple times.
 */
{
    char *          tp;
    DEFBUF **       prevp;
    DEFBUF *        defp;
    DEFBUF *        dp;
    int             cmp;
    size_t          s_name, s_def;

    if (skip_ws() == '('
            && scan_token( skip_ws(), (tp = work_buf, &tp), work_end) == STR
            && skip_ws() == ')') {          /* Correct syntax       */

        if (is_junk())
            return;
        s_name = strlen( work_buf) - 2;
        *(work_buf + s_name + 1) = '\0';
        memcpy( identifier, work_buf + 1, s_name + 1);
                                            /* Remove enclosing '"' */
        prevp = look_prev( identifier, &cmp);
        if (cmp == 0) { /* Current definition or pushed definition exists   */
            defp = *prevp;
            if (direction == PUSH) {/* #pragma push_macro( "MACRO") */
                if (defp->push) {           /* No current definition*/
                    if (warn_level & 1)
                        cwarn( "\"%s\" is already pushed"   /* _W1_ */
                                , identifier, 0L, NULL);
                    return;
                }
                /* Else the current definition exists.  Push it     */
                s_def = sizeof (DEFBUF) + 3 + s_name
                        + strlen( defp->repl) + strlen( defp->fname);
                s_def += strlen( defp->parmnames);
                dp = (DEFBUF *) xmalloc( s_def);
                memcpy( dp, defp, s_def);   /* Copy the definition  */
                dp->link = *prevp;          /* Insert to linked-list*/
                *prevp = dp;                /*      the pushed def  */
                prevp = &dp->link;          /* Next link to search  */
            } else {                /* #pragma pop_macro( "MACRO")  */
                if (defp->push == 0) {      /* Current definition   */
                    if (defp->link == NULL
                            || ! str_eq( identifier, defp->link->name)) {
                        if (warn_level & 1)
                            cwarn( "\"%s\" has not been pushed"     /* _W1_ */
                                    , identifier, 0L, NULL);
                        return;
                    } else {
                        *prevp = defp->link;
                                /* Link the previous and the next   */
                        free( defp);
                            /* Delete the definition to enable popped def   */
                    }
                }   /* Else no current definition exists    */
            }
            while ((defp = *prevp) != NULL) {
                /* Increment or decrement "push" count of all pushed defs   */
                if ((cmp = memcmp( defp->name, identifier, s_name)) > 0)
                    break;
                defp->push += direction;        /* Increment or decrement   */
                prevp = &defp->link;
            }
        } else {    /* No current definition nor pushed definition  */
            if (warn_level & 1)
                cwarn( "\"%s\" has not been defined"        /* _W1_ */
                        , identifier, 0L, NULL);
        }
    } else {        /* Wrong syntax */
        if (warn_level & 1)
            cwarn( "Bad %s syntax", direction == PUSH       /* _W1_ */
                    ? "push_macro" : "pop_macro", 0L, NULL);
    }
}


void    do_old( void)
/*
 * Process the out-of-standard directives.
 * GCC permits #include_next and #warning even in STANDARD mode.
 */
{
    static const char * const   unknown
            = "Unknown #directive \"%s\"%.0ld%s";       /* _E_ _W8_ */

    if (compiling) {
        cerror( unknown, identifier, 0L, NULL);
    } else if (warn_level & 8) {
        cwarn( unknown, identifier, 0L, " (in skipped block)");
    }
    skip_nl();
    unget_ch();
    return;
}

static void do_preprocessed( void)
/*
 * The source file has been already preprocessed.
 * Copy the lines to output.
 * Install macros according the #define directives.
 */
{
    const char *    corrupted =
            "This preprocessed file is corrupted";          /* _F_  */
    FILEINFO *      file;
    char *          lbuf;
    char *          cp;
    const char **   incptr;
    char *          comment = NULL;
    char *          colon = NULL;
    const char *    dir;
#if STD_LINE_PREFIX == FALSE
    char            conv[ NBUFF];
    char *          arg;

    /*
     * Compiler cannot accept C source style #line.
     * Convert it to the compiler-specific format.
     */
    strcpy( conv, LINE_PREFIX);
    arg = conv + strlen( conv);
#endif
    file = infile;
    lbuf = file->bptr = file->buffer;           /* Reset file->bptr */

    /* Copy the input to output until a comment line appears.       */
    while (fgets( lbuf, NBUFF, file->fp) != NULL
            && memcmp( lbuf, "/*", 2) != 0) {
#if STD_LINE_PREFIX == FALSE
        if (memcmp( lbuf, "#line ", 6) == 0) {
            strcpy( arg, lbuf + 6);
            mcpp_fputs( conv, OUT);
        } else
#endif
        {
            mcpp_fputs( lbuf, OUT);
        }
    }
    if (! str_eq( lbuf, "/* Currently defined macros. */\n"))
        cfatal( "This is not a preprocessed source"         /* _F_  */
                , NULL, 0L, NULL);

    /* Define macros according to the #define lines.    */
    while (fgets( lbuf, NWORK, file->fp) != NULL) {
        if (memcmp( lbuf, "/*", 2) == 0) {
                                    /* Standard predefined macro    */
            continue;
        }
        if (memcmp( lbuf, "#define ", 8) != 0) {
            if (memcmp( lbuf, "#line", 5) == 0)
                continue;
            else
                cfatal( corrupted, NULL, 0L, NULL);
        }
        /* Filename and line-number information in comment as:  */
        /* dir/fname:1234\t*/
        cp = lbuf + strlen( lbuf);
        if ((memcmp( cp - 4, "\t*/\n", 4) != 0)
                || (*(cp - 4) = EOS
                        , (comment = strrchr( lbuf, '*')) == NULL)
                || (memcmp( --comment, "/* ", 3) != 0)
                || ((colon = strrchr( comment, ':')) == NULL))
            cfatal( corrupted, NULL, 0L, NULL);
        src_line = atol( colon + 1);        /* Pseudo line number   */
        *colon = EOS;
        dir = comment + 3;
        inc_dirp = &null;
        /* Search the include directory list    */
        for (incptr = incdir ; incptr < incend; incptr++) {
            if (memcmp( *incptr, dir, strlen( *incptr)) == 0) {
                inc_dirp = incptr;
                break;
            }
        }
        /* Register the filename to fnamelist[] */
        /* inc_dirp may be NULL, and cur_fname may be "(predefined)"    */
        cur_fname = set_fname( dir + strlen( *inc_dirp));
        strcpy( comment - 2, "\n");         /* Remove the comment   */
        unget_string( lbuf + 8, NULL);
        do_define( FALSE, 0);
        get_ch();                               /* '\n' */
        get_ch();                               /* Clear the "file" */
        unget_ch();                             /* infile == file   */
    }
    file->bptr = file->buffer + strlen( file->buffer);
}

static int  do_debug(
    int     set                         /* TRUE to set debugging    */
)
/*
 * #pragma MCPP debug, #pragma MCPP end_debug, #debug, #end_debug
 * Return TRUE when diagnostic is issued else return FALSE.
 */
{
    struct Debug_arg {
        const char *    arg_name;               /* Name of option   */
        int     arg_num;                        /* Value of 'debug' */
    };
    static struct Debug_arg     debug_args[] = {
        { "path",   PATH    },
        { "token",  TOKEN   },
        { "expand", EXPAND  },
        { "macro_call", MACRO_CALL  },      /* Implemented only in STD mode */
        { "if",     IF      },
        { "expression", EXPRESSION  },
        { "getc",   GETC    },
        { "memory", MEMORY  },
        { NULL,     0       },
    };
    struct Debug_arg    *argp;
    int     num;
    int     c;

    c = skip_ws();
    if (c == '\n') {
        unget_ch();
        if (set) {
            if (warn_level & 1)
                cwarn( "No argument", NULL, 0L, NULL);      /* _W1_ */
            return TRUE;
        } else {
            mcpp_debug = 0;                 /* Clear all the flags  */
            return FALSE;
        }
    }
    while (scan_token( c, (workp = work_buf, &workp), work_end) == NAM) {
        argp = debug_args;
        while (argp->arg_name) {
            if (str_eq( argp->arg_name, work_buf))
                break;
            argp++;
        }
        if (argp->arg_name == NULL) {
            if (warn_level & 1)
                cwarn( unknown_arg, work_buf, 0L, NULL);
            goto  diagnosed;
        } else {
            num = argp->arg_num;
            if (set) {
                mcpp_debug |= num;
                if (num == PATH)
                    dump_path();
                else if (num == MEMORY)
                    print_heap();
                else if (num == MACRO_CALL)
                    // option_flags.k = TRUE;  /* This pragma needs this mode  */
                    ;
            } else {
                mcpp_debug &= ~num;
            }
        }
        c = skip_ws();
    }
    if (c != '\n') {
        if (warn_level & 1) {
            if (c != '\n') {
                cwarn( not_ident, work_buf, 0L, NULL);
            } else {
                cwarn( unknown_arg, work_buf, 0L, NULL);
                mcpp_debug &= ~num;                     /* Disable  */
            }
        }
        skip_nl();
        unget_ch();
        goto  diagnosed;
    }
    unget_ch();
    return FALSE;
diagnosed:
    return TRUE;
}

static void dump_path( void)
/*
 * Show the include directories.
 */
{
    const char **   incptr;
    const char *    inc_dir;
    const char *    dir = "./";

    mcpp_fputs( "Include paths are as follows --\n", DBG);
    for (incptr = incdir; incptr < incend; incptr++) {
        inc_dir = *incptr;
        if (*inc_dir == '\0')
            inc_dir = dir;
        mcpp_fprintf( DBG, "    %s\n", inc_dir);
    }
    mcpp_fputs( "End of include path list.\n", DBG);
}

/*
 * Note: The getopt() of glibc should not be used since the specification
 *  differs from the standard one.
 *  Use this mcpp_getopt() for mcpp.
 */

/* Based on the public-domain-software released by AT&T in 1985.    */

#define OPTERR( s, c)   if (mcpp_opterr) {  \
    mcpp_fputs( argv[0], ERR);  \
    mcpp_fputs( s, ERR);        \
    mcpp_fputc( c, ERR);        \
    mcpp_fputc( '\n', ERR);     \
    }

static int  mcpp_getopt(
    int         argc,
    char * const *  argv,
    const char *    opts
)
/*
 * Get the next option (and it's argument) from the command line.
 */
{
    const char * const   error1 = ": option requires an argument --";
    const char * const   error2 = ": illegal option --";
    static int      sp = 1;
    int             c;
    const char *    cp;

    if (sp == 1) {
        if (argc <= mcpp_optind ||
                argv[ mcpp_optind][ 0] != '-'
                    || argv[ mcpp_optind][ 1] == '\0') {
            return  EOF;
        } else if (strcmp( argv[ mcpp_optind], "--") == 0) {
            mcpp_optind++;
            return  EOF;
        }
    }
/*  mcpp_optopt = c = (unsigned char) argv[ mcpp_optind][ sp];  */
    mcpp_optopt = c = argv[ mcpp_optind][ sp] & UCHARMAX;
    if (c == ':' || (cp = strchr( opts, c)) == NULL) {
        OPTERR( error2, c)
        if (argv[ mcpp_optind][ ++sp] == '\0') {
            mcpp_optind++;
            sp = 1;
        }
        return  '?';
    }
    if (*++cp == ':') {
        if (argv[ mcpp_optind][ sp+1] != '\0') {
            mcpp_optarg = &argv[ mcpp_optind++][ sp+1];
        } else if (argc <= ++mcpp_optind) {
            OPTERR( error1, c)
            sp = 1;
            return  '?';
        } else {
            mcpp_optarg = argv[ mcpp_optind++];
        }
        sp = 1;
    } else {
        if (argv[ mcpp_optind][ ++sp] == '\0') {
            sp = 1;
            mcpp_optind++;
        }
        mcpp_optarg = NULL;
    }
    return  c;
}

#if ! HOST_HAVE_STPCPY
char *  stpcpy(
    char *          dest,
    const char *    src
)
/*
 * Copy the string and return the advanced pointer.
 */
{
    const char * s;
    char *  d;

    for (s = src, d = dest; (*d++ = *s++) != '\0'; )
        ;
    return  d - 1;
}
#endif

/*
 * list_heap() is a function to print out information of heap-memory.
 * See "kmmalloc-2.5.3.zip" by kmatsui.
 */
#if     KMMALLOC
    int     list_heap( int);
#elif   BSD_MALLOC
    int     list_heap( char *);
#elif   DB_MALLOC || DMALLOC
    int     list_heap( FILE *);
#endif

void    print_heap( void)
{
#if     KMMALLOC
    list_heap( 1);
#elif   BSD_MALLOC
    list_heap( ":cpp");
#elif   DB_MALLOC || DMALLOC || PHK_MALLOC || DLMALLOC
    list_heap( fp_debug);
#endif
}

void    at_end( void)
/*
 * Handle the commands to be executed at the end of processing.
 */
{
}

void    clear_filelist( void)
/*
 * Free malloced memory for filename-list and directory-list.
 */
{
    const char **   incp;
    INC_LIST *  namep;

    for (incp = incdir; incp < incend; incp++)
        free( (void *) *incp);
    free( (void *) incdir);
    for (namep = fnamelist; namep < fname_end; namep++)
        free( (void *) namep->name);
    free( (void *) fnamelist);
    free( (void *) once_list);
}

#if 0
// moved to a separate file because of Windows.h conflicts with other includes
#ifdef _WIN32
#  include <Windows.h>
#endif

FILE* mcpp_fopen(const char* filename, const char* mode)
{
#ifdef _WIN32
    FILE* f = 0;
    if(filename && mode)
    {
        int wfilenameLength = strlen(filename) + 1;
        wchar_t* wfilename = malloc(wfilenameLength * sizeof(wchar_t));
        if(wfilename)
        {
            if(MultiByteToWideChar(CP_UTF8, 0, filename, -1, wfilename, wfilenameLength))
            {
                int wmodeLength = strlen(mode) + 1;
                wchar_t* wmode = malloc(wmodeLength * sizeof(wchar_t));
                if(wmode)
                {
                    if(MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, wmodeLength))
                    {
                        _wfopen_s(&f, wfilename, wmode);
                    }
                    free(wmode);
                }
            }
            free(wfilename);
        }
    }
    return f;
#else
    return fopen(filename, mode);
#endif
}
#endif
