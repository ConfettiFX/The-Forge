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
 *                          M B C H A R . C
 *      C h a r a c t e r    h a n d l i n g    R o u t i n e s
 *
 * Character handling and multi-byte character handling routines are
 * placed here.
 */

#include    "system.H"
#include    "internal.H"

/*
 * Tables of character types and multi-byte character types.
 */

/* Non-ASCII characters are always checked by mb_read().    */
#define NA      0x4000  /* Non-ASCII characters */

/* Horizontal spaces (' ', '\t' and TOK_SEP)    */
#define HSPA    (SPA | HSP)

short *     char_type;  /* Pointer to one of the following type_*[].    */

/*
 * For UTF8 multi-byte character encoding.
 */

#define U2_1    0x100       /* 1st byte of 2-byte encoding of UTF8  */
#define U3_1    0x200       /* 1st byte of 3-byte encoding of UTF8  */
#define U4_1    0x400       /* 1st byte of 4-byte encoding of UTF8  */
#define UCONT   0x800   /* Continuation of a 2, 3, or 4 byte UTF8 sequence  */
#define U2_1N   (NA | U2_1)
#define U3_1N   (NA | U3_1)
#define U4_1N   (NA | U4_1)
#define UCONTN  (NA | UCONT)

static short    type_utf8[ UCHARMAX + 1] = {

/* Character type codes */
/*   0,     1,     2,     3,     4,     5,     6,     7,                    */
/*   8,     9,     A,     B,     C,     D,     E,     F,       Hex          */

   000,   000,   000,   000,   000,   000,   000,   000,    /* 00           */
   000,   HSPA,  SPA,   SPA,   SPA,   SPA,   000,   000,    /* 08           */
   000,   000,   000,   000,   000,   000,   000,   000,    /* 10           */
    /* 0x17-0x1A and 0x1F will be cleared in some modes by chk_opts()       */
   000,   LET,   LET,   000,   000,   000,   000,   HSPA,   /* 18           */
   HSPA,  PUNC,  QUO,   PUNC,  000,   PUNC,  PUNC,  QUO,    /* 20  !"#$%&'  */
   PUNC,  PUNC,  PUNC,  PUNC,  PUNC,  PUNC,  DOT,   PUNC,   /* 28 ()*+,-./  */
   DIG,   DIG,   DIG,   DIG,   DIG,   DIG,   DIG,   DIG,    /* 30 01234567  */
   DIG,   DIG,   PUNC,  PUNC,  PUNC,  PUNC,  PUNC,  PUNC,   /* 38 89:;<=>?  */

   000,   LET,   LET,   LET,   LET,   LET,   LET,   LET,    /* 40 @ABCDEFG  */
   LET,   LET,   LET,   LET,   LET,   LET,   LET,   LET,    /* 48 HIJKLMNO  */
   LET,   LET,   LET,   LET,   LET,   LET,   LET,   LET,    /* 50 PQRSTUVW  */
   LET,   LET,   LET,   PUNC,  000,   PUNC,  PUNC,  LET,    /* 58 XYZ[\]^_  */
   000,   LET,   LET,   LET,   LET,   LET,   LET,   LET,    /* 60 `abcdefg  */
   LET,   LET,   LET,   LET,   LET,   LET,   LET,   LET,    /* 68 hijklmno  */
   LET,   LET,   LET,   LET,   LET,   LET,   LET,   LET,    /* 70 pqrstuvw  */
   LET,   LET,   LET,   PUNC,  PUNC,  PUNC,  PUNC,  000,    /* 78 xyz{|}~   */

   UCONTN,UCONTN,UCONTN,UCONTN,UCONTN,UCONTN,UCONTN,UCONTN, /*   80 .. 87   */
   UCONTN,UCONTN,UCONTN,UCONTN,UCONTN,UCONTN,UCONTN,UCONTN, /*   88 .. 8F   */
   UCONTN,UCONTN,UCONTN,UCONTN,UCONTN,UCONTN,UCONTN,UCONTN, /*   90 .. 97   */
   UCONTN,UCONTN,UCONTN,UCONTN,UCONTN,UCONTN,UCONTN,UCONTN, /*   98 .. 9F   */
   UCONTN,UCONTN,UCONTN,UCONTN,UCONTN,UCONTN,UCONTN,UCONTN, /*   A0 .. A7   */
   UCONTN,UCONTN,UCONTN,UCONTN,UCONTN,UCONTN,UCONTN,UCONTN, /*   A8 .. AF   */
   UCONTN,UCONTN,UCONTN,UCONTN,UCONTN,UCONTN,UCONTN,UCONTN, /*   B0 .. B7   */
   UCONTN,UCONTN,UCONTN,UCONTN,UCONTN,UCONTN,UCONTN,UCONTN, /*   B8 .. BF   */
   NA,    NA,    U2_1N, U2_1N, U2_1N, U2_1N, U2_1N, U2_1N,  /*   C0 .. C7   */
   U2_1N, U2_1N, U2_1N, U2_1N, U2_1N, U2_1N, U2_1N, U2_1N,  /*   C8 .. CF   */
   U2_1N, U2_1N, U2_1N, U2_1N, U2_1N, U2_1N, U2_1N, U2_1N,  /*   D0 .. D7   */
   U2_1N, U2_1N, U2_1N, U2_1N, U2_1N, U2_1N, U2_1N, U2_1N,  /*   D8 .. DF   */
   U3_1N, U3_1N, U3_1N, U3_1N, U3_1N, U3_1N, U3_1N, U3_1N,  /*   E0 .. E7   */
   U3_1N, U3_1N, U3_1N, U3_1N, U3_1N, U3_1N, U3_1N, U3_1N,  /*   E8 .. EF   */
   U4_1N, U4_1N, U4_1N, U4_1N, U4_1N, NA,    NA,    NA,     /*   F0 .. F7   */
   NA,    NA,    NA,    NA,    NA,    NA,    NA,    NA,     /*   F8 .. FF   */
};

static int      mbstart;

static size_t   mb_read_utf8( int c1, char ** in_pp, char ** out_pp);
                /* For UTF8 mbchar encoding         */

void    mb_init()
/*
 * Initialize multi-byte character settings.
 * First called prior to setting the 'mcpp_mode'.
 * Will be called again each time the multibyte character encoding is changed.
 */
{
    /*
     * Select the character classification table, select the multi-byte
     * character reading routine and decide whether multi-byte character
     * may contain the byte of value 0x5c.
     */
     char_type = type_utf8;
     mb_read = mb_read_utf8;
     mbstart = (U2_1 | U3_1 | U4_1);
     mbchk = NA;

    /*
     * Modify magic characters in character type table.
     * char_type[] table should be rewritten in accordance with the 'mcpp_mode'
     * whenever the encoding is changed.
     */
     char_type[ DEF_MAGIC] = LET;
     char_type[ IN_SRC] = LET;
     char_type[ TOK_SEP] = HSPA;          /* TOK_SEP equals to COM_SEP    */
}

static size_t   mb_read_utf8(
    int     c1,
    char ** in_pp,
    char ** out_pp
)
/*
 * Multi-byte character reading routine for UTF8.
 */
{
    int     error = FALSE;
    size_t  len = 0;
    char *  in_p = *in_pp;
    char *  out_p = *out_pp;

    if (! (char_type[ c1 & UCHARMAX] & mbstart))
        return  MB_ERROR;

    do {
        unsigned int    codepoint;
        int             i, bytes;

        if ((char_type[ c1 & UCHARMAX] & U4_1) == U4_1)
            bytes = 4;                          /* 4-byte character */
        else if ((char_type[ c1 & UCHARMAX] & U3_1) == U3_1)
            bytes = 3;                          /* 3-byte character */
        else //if ((char_type[ c1 & UCHARMAX] & U2_1) == U2_1)
            bytes = 2;                          /* 2-byte character */

        /* Must ensure that the sequence is not reserved as a surrogate */
        codepoint = ((2 << (6-bytes)) - 1) & c1;    /* mask off top bits    */

        /* All bytes left in the sequence must be in 0x80 - 0xBF    */
        for (i = bytes - 1; i && !error; i--) {
            codepoint = (codepoint << 6) + ((*in_p) & 0x3fU);
            if (! (char_type[ (*out_p++ = *in_p++) & UCHARMAX] & UCONT))
                error = TRUE;
        }

        /* Check for overlong/underlong sequences */
        if ((bytes == 2 && (codepoint < 0x80 || codepoint > 0x7FF))
            || (bytes == 3 && (codepoint < 0x800 || codepoint > 0xFFFF))
            || (bytes == 4 && (codepoint < 0x10000 || codepoint > 0x10FFFF)))
            error = TRUE;
        if ((codepoint >= 0xD800 && codepoint <= 0xDFFF)
            /* Check for reserved surrogate codepoints */
                || (codepoint >= 0xFFFE && codepoint <= 0xFFFF))
                /* Illegal  */
            error = TRUE;
#if 0
        printf( "codepoint:0x%x\n", codepoint);
#endif
        if (error)
            break;
        len++;
    } while (char_type[ (*out_p++ = c1 = *in_p++) & UCHARMAX] & mbstart);
                        /* Start of the next multi-byte character   */
    *in_pp = --in_p;
    *(--out_p) = EOS;
    *out_pp = out_p;
    return  error ? (len | MB_ERROR) : len;
}

uexpr_t     mb_eval(
    char ** seq_pp
)
/*
 * Evaluate the value of a multi-byte character.
 * This routine does not check the legality of the sequence.
 * This routine is called from eval_char().
 * This routine is never called in POST_STD mode.
 */
{
    char *      seq = *seq_pp;
    uexpr_t     val = 0;
    int         c;

    if (! (char_type[ c = *seq++ & UCHARMAX] & mbstart)) {
        *seq_pp = seq;
        return  c;                  /* Not a multi-byte character   */
    }

     val = (c << 8) + (*seq++ & UCHARMAX);
     if (char_type[ c & UCHARMAX] & U3_1) {
         val = (val << 8) + (*seq++ & UCHARMAX);
     } else if (char_type[ c & UCHARMAX] & U4_1) {
         val = (val << 8) + (*seq++ & UCHARMAX);
         val = (val << 8) + (*seq++ & UCHARMAX);
     }

    *seq_pp = seq;
    return  val;
}
