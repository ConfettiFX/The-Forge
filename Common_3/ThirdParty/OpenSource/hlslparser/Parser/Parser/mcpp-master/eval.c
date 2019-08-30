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
 *                              E V A L . C
 *                  E x p r e s s i o n   E v a l u a t i o n
 *
 * The routines to evaluate #if expression are placed here.
 * Some routines are used also to evaluate the value of numerical tokens.
 */

 /* forge edits */
#pragma warning(disable:4996)
#pragma warning(disable:4267)
#pragma warning(disable:4244)
#pragma warning(disable:4146)

#include    "system.H"
#include    "internal.H"

typedef struct optab {
    char    op;                     /* Operator                     */
    char    prec;                   /* Its precedence               */
    char    skip;                   /* Short-circuit: non-0 to skip */
} OPTAB;

static int      eval_lex( void);
                /* Get type and value of token  */
static int      chk_ops( void);
                /* Check identifier-like ops    */
static VAL_SIGN *   eval_char( char * const token);
                /* Evaluate character constant  */
static expr_t   eval_one( char ** seq_pp, int wide, int mbits, int * ucn8);
                /* Evaluate a character         */
static VAL_SIGN *   eval_eval( VAL_SIGN * valp, int op);
                /* Entry to #if arithmetic      */
static expr_t   eval_signed( VAL_SIGN ** valpp, expr_t v1, expr_t v2, int op);
                /* Do signed arithmetic of expr.*/
static expr_t   eval_unsigned( VAL_SIGN ** valpp, uexpr_t v1u, uexpr_t v2u
        , int op);
                /* Do unsigned arithmetic       */
static void     overflow( const char * op_name, VAL_SIGN ** valpp
        , int ll_overflow);
                /* Diagnose overflow of expr.   */
static void     dump_val( const char * msg, const VAL_SIGN * valp);
                /* Print value of an operand    */
static void     dump_stack( const OPTAB * opstack, const OPTAB * opp
        , const VAL_SIGN * value, const VAL_SIGN * valp);
                /* Print stacked operators      */

/* For debug and error messages.    */
static const char * const   opname[ OP_END + 1] = {
    "end of expression",    "val",  "(",
    "unary +",      "unary -",      "~",    "!",
    "*",    "/",    "%",
    "+",    "-",    "<<",   ">>",
    "<",    "<=",   ">",    ">=",   "==",   "!=",
    "&",    "^",    "|",    "&&",   "||",
    "?",    ":",
    ")",    "(none)"
};

/*
 * opdope[] has the operator (and operand) precedence:
 *     Bits
 *        7     Unused (so the value is always positive)
 *      6-2     Precedence (0000 .. 0174)
 *      1-0     Binary op. flags:
 *          10  The next binop flag (binop should/not follow).
 *          01  The binop flag (should be set/cleared when this op is seen).
 * Note:   next binop
 * value    1   0   Value doesn't follow value.
 *                  Binop, ), END should follow value, value or unop doesn't.
 *  (       0   0   ( doesn't follow value.  Value follows.
 * unary    0   0   Unop doesn't follow value.  Value follows.
 * binary   0   1   Binary op follows value.  Value follows.
 *  )       1   1   ) follows value.  Binop, ), END follows.
 * END      0   1   END follows value, doesn't follow ops.
 */

static const char   opdope[ OP_END + 1] = {
    0001,                               /* End of expression        */
    0002, 0170,                         /* VAL (constant), LPA      */
/* Unary op's   */
    0160, 0160, 0160, 0160,             /* PLU, NEG, COM, NOT       */
/* Binary op's  */
    0151, 0151, 0151,                   /* MUL, DIV, MOD,           */
    0141, 0141, 0131, 0131,             /* ADD, SUB, SL, SR         */
    0121, 0121, 0121, 0121, 0111, 0111, /* LT, LE, GT, GE, EQ, NE   */
    0101, 0071, 0061, 0051, 0041,       /* AND, XOR, OR, ANA, ORO   */
    0031, 0031,                         /* QUE, COL                 */
/* Parens       */
    0013, 0023                          /* RPA, END                 */
};
/*
 * OP_QUE, OP_RPA and unary operators have alternate precedences:
 */
#define OP_RPA_PREC     0013
#define OP_QUE_PREC     0024    /* From right to left grouping      */
#define OP_UNOP_PREC    0154            /*      ditto               */

/*
 * S_ANDOR and S_QUEST signal "short-circuit" boolean evaluation, so that
 *      #if FOO != 0 && 10 / FOO ...
 * doesn't generate an error message.  They are stored in optab.skip.
 */
#define S_ANDOR         2
#define S_QUEST         1

static VAL_SIGN     ev;     /* Current value and signedness     */
static int          skip = 0;   /* 3-way signal of skipping expr*/
static const char * const   non_eval
        = " (in non-evaluated sub-expression)";             /* _W8_ */

#if HAVE_LONG_LONG
    static int  w_level = 1;    /* warn_level at overflow of long   */
#else
    static int  w_level = 2;
#endif

/*
 * In KR and OLD_PREP modes.
 * Define bits for the basic types and their adjectives.
 */
#define T_CHAR          1
#define T_INT           2
#define T_FLOAT         4
#define T_DOUBLE        8
#define T_LONGDOUBLE    16
#define T_SHORT         32
#define T_LONG          64
#define T_LONGLONG      128
#define T_SIGNED        256
#define T_UNSIGNED      512
#define T_PTR           1024        /* Pointer to data objects      */
#define T_FPTR          2048        /* Pointer to functions         */

/*
 * The SIZES structure is used to store the values for #if sizeof.
 */
typedef struct sizes {
        int             bits;       /* If this bit is set,          */
        int             size;       /* this is the datum size value */
        int             psize;      /* this is the pointer size     */
} SIZES;

/*
 * S_CHAR, etc.  define the sizeof the basic TARGET machine word types.
 *      By default, sizes are set to the values for the HOST computer.  If
 *      this is inappropriate, see those tables for details on what to change.
 *      Also, if you have a machine where sizeof (signed int) differs from
 *      sizeof (unsigned int), you will have to edit those tables and code in
 *      eval.c.
 * Note: sizeof in #if expression is disallowed by Standard.
 */

#define S_CHAR      (sizeof (char))
#define S_SINT      (sizeof (short int))
#define S_INT       (sizeof (int))
#define S_LINT      (sizeof (long int))
#define S_FLOAT     (sizeof (float))
#define S_DOUBLE    (sizeof (double))
#define S_PCHAR     (sizeof (char *))
#define S_PSINT     (sizeof (short int *))
#define S_PINT      (sizeof (int *))
#define S_PLINT     (sizeof (long int *))
#define S_PFLOAT    (sizeof (float *))
#define S_PDOUBLE   (sizeof (double *))
#define S_PFPTR     (sizeof (int (*)()))
#if HAVE_LONG_LONG
#if (HOST_COMPILER == BORLANDC) \
        || (HOST_COMPILER == MSC && defined(_MSC_VER) && (_MSC_VER < 1300))
#define S_LLINT     (sizeof (__int64))
#define S_PLLINT    (sizeof (__int64 *))
#else
#define S_LLINT     (sizeof (long long int))
#define S_PLLINT    (sizeof (long long int *))
#endif
#endif
#define S_LDOUBLE   (sizeof (long double))
#define S_PLDOUBLE  (sizeof (long double *))

typedef struct types {
    int         type;               /* This is the bits for types   */
    char *      token_name;         /* this is the token word       */
    int         excluded;           /* but these aren't legal here. */
} TYPES;

#define ANYSIGN     (T_SIGNED | T_UNSIGNED)
#define ANYFLOAT    (T_FLOAT | T_DOUBLE | T_LONGDOUBLE)
#if HAVE_LONG_LONG
#define ANYINT      (T_CHAR | T_SHORT | T_INT | T_LONG | T_LONGLONG)
#else
#define ANYINT      (T_CHAR | T_SHORT | T_INT | T_LONG)
#endif

#define is_binary(op)   (FIRST_BINOP <= op && op <= LAST_BINOP)
#define is_unary(op)    (FIRST_UNOP  <= op && op <= LAST_UNOP)


void    init_eval( void)
{
    skip = 0;
}

expr_t  eval_if( void)
/*
 * Evaluate a #if expression.  Straight-forward operator precedence.
 * This is called from directive() on encountering an #if directive.
 * It calls the following routines:
 * eval_lex()   Lexical analyser -- returns the type and value of
 *              the next input token.
 * eval_eval()  Evaluates the current operator, given the values on the
 *              value stack.  Returns a pointer to the (new) value stack.
 */
{
    VAL_SIGN        value[ NEXP * 2 + 1];   /* Value stack          */
    OPTAB           opstack[ NEXP * 3 + 1]; /* Operator stack       */
    int             parens = 0;     /* Nesting levels of (, )       */
    int             prec;           /* Operator precedence          */
    int             binop = 0;      /* Set if binary op. needed     */
    int             op1;            /* Operator from stack          */
    int             skip_cur;       /* For short-circuit testing    */
    VAL_SIGN *      valp = value;   /* -> Value and signedness      */
    OPTAB *         opp = opstack;  /* -> Operator stack            */
    int             op;             /* Current operator             */

    opp->op = OP_END;               /* Mark bottom of stack         */
    opp->prec = opdope[ OP_END];    /* And its precedence           */
    skip = skip_cur = opp->skip = 0;        /* Not skipping now     */

    while (1) {
        if (mcpp_debug & EXPRESSION)
            mcpp_fprintf( DBG
                    , "In eval loop skip = %d, binop = %d, line is: %s\n"
                    , opp->skip, binop, infile->bptr);
        skip = opp->skip;
        op = eval_lex();
        skip = 0;                   /* Reset to be ready to return  */
        switch (op) {
        case OP_SUB :
            if (binop == 0)
                op = OP_NEG;                /* Unary minus          */
            break;
        case OP_ADD :
            if (binop == 0)
                op = OP_PLU;                /* Unary plus           */
            break;
        case OP_FAIL:
            return  0L;                     /* Token error          */
        }
        if (mcpp_debug & EXPRESSION)
            mcpp_fprintf( DBG
                    , "op = %s, opdope = %04o, binop = %d, skip = %d\n"
                    , opname[ op], opdope[ op], binop, opp->skip);
        if (op == VAL) {                    /* Value?               */
            if (binop != 0) {               /* Binop is needed      */
                cerror( "Misplaced constant \"%s\""         /* _E_  */
                        , work_buf, 0L, NULL);
                return  0L;
            } else if (& value[ NEXP * 2] <= valp) {
                cerror( "More than %.0s%ld constants stacked at %s" /* _E_  */
                        , NULL, (long) (NEXP * 2 - 1), work_buf);
                return  0L;
            } else {
                if (mcpp_debug & EXPRESSION) {
                    dump_val( "pushing ", &ev);
                    mcpp_fprintf( DBG, " onto value stack[%d]\n"
                            , (int)(valp - value));
                }
                valp->val = ev.val;
                (valp++)->sign = ev.sign;
                binop = 1;  /* Binary operator or so should follow  */
            }
            continue;
        }                                   /* Else operators       */
        prec = opdope[ op];
        if (binop != (prec & 1)) {
            if (op == OP_EOE)
                cerror( "Unterminated expression"           /* _E_  */
                        , NULL, 0L, NULL);
            else
                cerror( "Operator \"%s\" in incorrect context"      /* _E_  */
                        , opname[ op], 0L, NULL);
            return  0L;
        }
        binop = (prec & 2) >> 1;            /* Binop should follow? */

        while (1) {
            if (mcpp_debug & EXPRESSION)
                mcpp_fprintf( DBG
                        , "op %s, prec %d, stacked op %s, prec %d, skip %d\n"
                , opname[ op], prec, opname[ (int)opp->op], opp->prec, opp->skip);

            /* Stack coming sub-expression of higher precedence.    */
            if (opp->prec < prec) {
                if (op == OP_LPA) {
                    prec = OP_RPA_PREC;
                    if ((warn_level & 4)
                            && ++parens == std_limits.exp_nest + 1)
                        cwarn(
                    "More than %.0s%ld nesting of parens"   /* _W4_ */
                            , NULL, (long) std_limits.exp_nest, NULL);
                } else if (op == OP_QUE) {
                    prec = OP_QUE_PREC;
                } else if (is_unary( op)) {
                    prec = OP_UNOP_PREC;
                }
                op1 = opp->skip;            /* Save skip for test   */
                /*
                 * Push operator onto operator stack.
                 */
                opp++;
                if (& opstack[ NEXP * 3] <= opp) {
                    cerror(
            "More than %.0s%ld operators and parens stacked at %s"  /* _E_  */
                            , NULL, (long) (NEXP * 3 - 1), opname[ op]);
                    return  0L;
                }
                opp->op = op;
                opp->prec = prec;
                if (&value[0] < valp)
                    skip_cur = (valp[-1].val != 0L);
                                            /* Short-circuit tester */
                /*
                 * Do the short-circuit stuff here.  Short-circuiting
                 * stops automagically when operators are evaluated.
                 */
                if ((op == OP_ANA && ! skip_cur)
                        || (op == OP_ORO && skip_cur)) {
                    opp->skip = S_ANDOR;    /* And/or skip starts   */
                    if (skip_cur)           /* Evaluate non-zero    */
                        valp[-1].val = 1L;  /*   value to 1         */
                } else if (op == OP_QUE) {  /* Start of ?: operator */
                    opp->skip = (op1 & S_ANDOR) | (!skip_cur ? S_QUEST : 0);
                } else if (op == OP_COL) {  /* : inverts S_QUEST    */
                    opp->skip = (op1 & S_ANDOR)
                              | (((op1 & S_QUEST) != 0) ? 0 : S_QUEST);
                } else {                    /* Other operators leave*/
                    opp->skip = op1;        /*  skipping unchanged. */
                }
                if (mcpp_debug & EXPRESSION) {
                    mcpp_fprintf( DBG, "stacking %s, ", opname[ op]);
                    if (&value[0] < valp)
                        dump_val( "valp[-1].val == ", valp - 1);
                    mcpp_fprintf( DBG, " at %s\n", infile->bptr);
                    dump_stack( opstack, opp, value, valp);
                }
                break;
            }

            /*
             * Coming sub-expression is of lower precedence.
             * Evaluate stacked sub-expression.
             * Pop operator from operator stack and evaluate it.
             * End of stack and '(', ')' are specials.
             */
            skip_cur = opp->skip;           /* Remember skip value  */
            switch ((op1 = opp->op)) {      /* Look at stacked op   */
            case OP_END:                    /* Stack end marker     */
                if (op == OP_RPA) {         /* No corresponding (   */
                    cerror( "Excessive \")\"", NULL, 0L, NULL);     /* _E_  */
                    return  0L;
                }
                if (op == OP_EOE)
                    return  valp[-1].val;   /* Finished ok.         */
                break;
            case OP_LPA:                    /* ( on stack           */
                if (op != OP_RPA) {         /* Matches ) on input?  */
                    cerror( "Missing \")\"", NULL, 0L, NULL);       /* _E_  */
                    return  0L;
                }
                opp--;                      /* Unstack it           */
                parens--;                   /* Count down nest level*/
                break;
            case OP_QUE:                    /* Evaluate true expr.  */
                break;
            case OP_COL:                    /* : on stack           */
                opp--;                      /* Unstack :            */
                if (opp->op != OP_QUE) {    /* Matches ? on stack?  */
                    cerror(
                    "Misplaced \":\", previous operator is \"%s\""  /* _E_  */
                            , opname[(int)opp->op], 0L, NULL);
                    return  0L;
                }
                /* Evaluate op1.            Fall through            */
            default:                        /* Others:              */
                opp--;                      /* Unstack the operator */
                if (mcpp_debug & EXPRESSION) {
                    mcpp_fprintf( DBG, "Stack before evaluation of %s\n"
                            , opname[ op1]);
                    dump_stack( opstack, opp, value, valp);
                }
                if (op1 == OP_COL)
                    skip = 0;
                else
                    skip = skip_cur;
                valp = eval_eval( valp, op1);
                if (valp->sign == VAL_ERROR)
                    return  0L;     /* Out of range or divide by 0  */
                valp++;
                skip = 0;
                if (mcpp_debug & EXPRESSION) {
                    mcpp_fprintf( DBG, "Stack after evaluation\n");
                    dump_stack( opstack, opp, value, valp);
                }
            }                               /* op1 switch end       */

            if (op1 == OP_END || op1 == OP_LPA || op1 == OP_QUE)
                break;                      /* Read another op.     */
        }                                   /* Stack unwind loop    */

    }

    return  0L;                             /* Never reach here     */
}

static int  eval_lex( void)
/*
 * Return next operator or constant to evaluate.  Called from eval_if().  It 
 * calls a special-purpose routines for character constants and numeric values:
 *      eval_char()     called to evaluate 'x'
 *      eval_num()      called to evaluate numbers
 * C++98 treats 11 identifier-like tokens as operators.
 * POST_STD forbids character constants in #if expression.
 */
{
    int     c1;
    VAL_SIGN *  valp;
    int     warn = ! skip || (warn_level & 8);
    int     token_type;
    int     c;

    ev.sign = SIGNED;                       /* Default signedness   */
    ev.val = 0L;            /* Default value (on error or 0 value)  */
    in_if = ! skip; /* Inform to expand_macro() that the macro is   */
                    /*      in #if line and not skipped expression. */
    c = skip_ws();
    if (c == '\n') {
        unget_ch();
        return  OP_EOE;                     /* End of expression    */
    }
    token_type = get_unexpandable( c, warn);
    if (macro_line == MACRO_ERROR)
        return  OP_FAIL;                /* Unterminated macro call  */
    if (token_type == NO_TOKEN)
        return  OP_EOE;     /* Only macro(s) expanding to 0-token   */

    switch (token_type) {
    case NAM:
        if (str_eq( identifier, "defined")) {   /* defined name */
            c1 = c = skip_ws();
            if (c == '(')                   /* Allow defined (name) */
                c = skip_ws();
            if (scan_token( c, (workp = work_buf, &workp), work_end) == NAM) {
                DEFBUF *    defp = look_id( identifier);
                if (warn) {
                    ev.val = (defp != NULL);
                    if ((mcpp_debug & MACRO_CALL) && ! skip && defp)
                        /* Annotate if the macro is in non-skipped expr.    */
                        mcpp_fprintf( OUT, "/*%s*/", defp->name);
                }
                if (c1 != '(' || skip_ws() == ')')  /* Balanced ?   */
                    return  VAL;            /* Parsed ok            */
            }
            cerror( "Bad defined syntax: %s"                /* _E_  */
                    , infile->fp ? "" : infile->buffer, 0L, NULL);
            break;
        }
        /*
         * The ANSI C Standard says that an undefined symbol
         * in an #if has the value zero.  We are a bit pickier,
         * warning except where the programmer was careful to write
         *          #if defined(foo) ? foo : 0
         */
        if ((! skip && (warn_level & 4)) || (skip && (warn_level & 8)))
            cwarn( "Undefined symbol \"%s\"%.0ld%s" /* _W4_ _W8_    */
                    , identifier, 0L, skip ? non_eval : ", evaluated to 0");
        return  VAL;
    case CHR:                               /* Character constant   */
    case WCHR:                              /* Wide char constant   */
        valp = eval_char( work_buf);        /* 'valp' points 'ev'   */
        if (valp->sign == VAL_ERROR)
            break;
        if (mcpp_debug & EXPRESSION) {
            dump_val( "eval_char returns ", &ev);
            mcpp_fputc( '\n', DBG);
        }
        return  VAL;                        /* Return a value       */
    case STR:                               /* String literal       */
    case WSTR:                              /* Wide string literal  */
        cerror(
    "Can't use a string literal %s", work_buf, 0L, NULL);   /* _E_  */
        break;
    case NUM:                               /* Numbers are harder   */
        valp = eval_num( work_buf);         /* 'valp' points 'ev'   */
        if (valp->sign == VAL_ERROR)
            break;
        if (mcpp_debug & EXPRESSION) {
            dump_val( "eval_num returns ", &ev);
            mcpp_fputc( '\n', DBG);
        }
        return  VAL;
    case OPE:                           /* Operator or punctuator   */
        return  chk_ops();

    default:                                /* Total nonsense       */
        cerror( "Can't use the character %.0s0x%02lx"       /* _E_  */
                , NULL, (long) c, NULL);
        break;
    }

    return  OP_FAIL;                        /* Any errors           */
}

static int  chk_ops( void)
/*
 * Check the operator.
 * If it can't be used in #if expression return OP_FAIL
 * else return openum.
 */
{
    switch (openum) {
    case OP_STR:    case OP_CAT:    case OP_ELL:
    case OP_1:      case OP_2:      case OP_3:
        cerror( "Can't use the operator \"%s\""             /* _E_  */
                , work_buf, 0L, NULL);
        return  OP_FAIL;
    default:
        return  openum;
    }
}

VAL_SIGN *  eval_num(
    const char *  nump                      /* Preprocessing number */
)
/*
 * Evaluate number for #if lexical analysis.  Note: eval_num recognizes
 * the unsigned suffix, but only returns a signed expr_t value, and stores
 * the signedness to ev.sign, which is set UNSIGNED (== unsigned) if the
 * value is not in the range of positive (signed) expr_t.
 */
{
    const char * const  not_integer = "Not an integer \"%s\"";      /* _E_  */
    const char * const  out_of_range
            = "Constant \"%s\"%.0ld%s is out of range"; /* _E_ _W1_ _W8_    */
    expr_t          value;
    uexpr_t         v, v1;  /* unsigned long long or unsigned long  */
    int             uflag = FALSE;
    int             lflag = FALSE;
    int             erange = FALSE;
    int             base;
    int             c, c1;
    const char *    cp = nump;
#if HAVE_LONG_LONG
    const char * const  out_of_range_long =
        "Constant \"%s\"%.0ld%s is out of range "   /* _E_ _W1_ _W2_ _W8_   */
            "of (unsigned) long";
    const char * const  ll_suffix =
"LL suffix is used in other than C99 mode \"%s\"%.0ld%s"; /* _W1_ _W2_ _W8_ */
    int             llflag = FALSE;
    int             erange_long = FALSE;
#endif

    ev.sign = SIGNED;                       /* Default signedness   */
    ev.val = 0L;                            /* Default value        */
    if ((char_type[ c = *cp++ & UCHARMAX] & DIG) == 0)   /* Dot     */
        goto  num_err;
    if (c != '0') {                         /* Decimal              */
        base = 10;
    } else if ((c = *cp++ & UCHARMAX) == 'x' || c == 'X') {
        base = 16;                          /* Hexadecimal          */
        c = *cp++ & UCHARMAX;
    } else if (c == EOS) {                  /* 0                    */
        return  & ev;
    } else {                                /* Octal or illegal     */
        base = 8;
    }

    v = v1 = 0L;
    for (;;) {
        c1 = c;
        if (isupper( c1))
            c1 = tolower( c1);
        if (c1 >= 'a')
            c1 -= ('a' - 10);
        else
            c1 -= '0';
        if (c1 < 0 || base <= c1)
            break;
        v1 *= base;
        v1 += c1;
        if (v1 / base < v) {                /* Overflow             */
            if (! skip)
                goto  range_err;
            else
                erange = TRUE;
        }
#if HAVE_LONG_LONG
        if (v1 > ULONGMAX)
            /* Overflow of long or unsigned long    */
            erange_long = TRUE;
#endif
        v = v1;
        c = *cp++ & UCHARMAX;
    }

    value = v;
    while (c == 'u' || c == 'U' || c == 'l' || c == 'L') {
        if (c == 'u' || c == 'U') {
            if (uflag)
                goto  num_err;
            uflag = TRUE;
        } else if (c == 'l' || c == 'L') {
#if HAVE_LONG_LONG
            if (llflag) {
                goto  num_err;
            } else if (lflag) {
                llflag = TRUE;
                if (((! skip && (warn_level & w_level))
                        || (skip && (warn_level & 8))))
                    cwarn( ll_suffix, nump, 0L, skip ? non_eval : NULL);
            } else {
                lflag = TRUE;
            }
#else
            if (lflag)
                goto  num_err;
            else
                lflag = TRUE;
#endif
        }
        c = *cp++;
    }
    if (c != EOS)
        goto  num_err;

    if (uflag)      /* If 'U' suffixed, uexpr_t is treated as unsigned  */
        ev.sign = UNSIGNED;
    else
        ev.sign = (value >= 0L);
#if HAVE_LONG_LONG
    if (value > LONGMAX)
        erange_long = TRUE;
#endif
    

    ev.val = value;
    if (erange && (warn_level & 8))
        cwarn( out_of_range, nump, 0L, non_eval);
#if HAVE_LONG_LONG
    else if (erange_long && ((skip && (warn_level & 8))
            || (! skip && (warn_level & w_level))))
        cwarn( out_of_range_long, nump, 0L, skip ? non_eval : NULL);
#endif
    return  & ev;

range_err:
    cerror( out_of_range, nump, 0L, NULL);
    ev.sign = VAL_ERROR;
    return  & ev;
num_err:
    cerror( not_integer, nump, 0L, NULL);
    ev.sign = VAL_ERROR;
    return  & ev;
}

static VAL_SIGN *   eval_char(
    char * const    token
)
/*
 * Evaluate a character constant.
 * This routine is never called in POST_STD mode.
 */
{
    const char * const  w_out_of_range
        = "Wide character constant %s%.0ld%s is out of range";  /* _E_ _W8_ */
    const char * const  c_out_of_range
    = "Integer character constant %s%.0ld%s is out of range";   /* _E_ _W8_ */
    uexpr_t     value;
    uexpr_t     tmp;
    expr_t      cl;
    int         erange = FALSE;
    int         wide = (*token == 'L');
    int         ucn8;
    int         i;
    int         bits, mbits;
    char *      cp = token + 1;         /* Character content        */
#if HAVE_LONG_LONG
    const char * const  w_out_of_range_long =
        "Wide character constant %s%.0ld%s is "     /* _E_ _W1_ _W2_ _W8_   */
            "out of range of unsigned long";
    const char * const  c_out_of_range_long =
        "Integer character constant %s%.0ld%s is "  /* _E_ _W1_ _W2_ _W8_   */
            "out of range of unsigned long";
    int         erange_long = FALSE;
#endif

    bits = CHARBIT;
    mbits = CHARBIT * 4;
    if (wide) {          /* Wide character constant  */
        cp++;                           /* Skip 'L'                 */
        bits = mbits;
    }
    if (char_type[ *cp & UCHARMAX] & mbchk) {
        cl = mb_eval( &cp);
        bits = mbits;
    } else if ((cl = eval_one( &cp, wide, mbits, (ucn8 = FALSE, &ucn8)))
                == -1L) {
        ev.sign = VAL_ERROR;
        return  & ev;
    }
    value = cl;

    for (i = 0; *cp != '\'' && *cp != EOS; i++) {
        if (char_type[ *cp & UCHARMAX] & mbchk) {
            cl = mb_eval( &cp);
            if (cl == 0)
                /* Shift-out sequence of multi-byte or wide character   */
                continue;
            bits = mbits;
        } else {
            cl = eval_one( &cp, wide, mbits, (ucn8 = FALSE, &ucn8));
            if (cl == -1L) {
                ev.sign = VAL_ERROR;
                return  & ev;
            }
        }
        tmp = value;
        value = (value << bits) | cl;   /* Multi-char or multi-byte char    */
        if ((value >> bits) < tmp) {    /* Overflow                 */
            if (! skip)
                goto  range_err;
            else
                erange = TRUE;
        }
    }

    ev.sign = ((expr_t) value >= 0L);
    ev.val = value;

    if (erange && skip && (warn_level & 8)) {
        if (wide)
            cwarn( w_out_of_range, token, 0L, non_eval);
        else
            cwarn( c_out_of_range, token, 0L, non_eval);
#if HAVE_LONG_LONG
    } else if (erange_long && ((skip && (warn_level & 8))
            || (! skip && (warn_level & w_level)))) {
        if (wide)
            cwarn( w_out_of_range_long, token, 0L, skip ? non_eval : NULL);
        else
            cwarn( c_out_of_range_long, token, 0L, skip ? non_eval : NULL);
#endif
    }

    if (i == 0)         /* Constant of single (character or wide-character) */
        return  & ev;

    if ((! skip && (warn_level & 4)) || (skip && (warn_level & 8))) {
        if (wide)
            cwarn(
"Multi-character wide character constant %s%.0ld%s isn't portable"  /* _W4_ _W8_    */
                    , token, 0L, skip ? non_eval : NULL);
        else
            cwarn(
"Multi-character or multi-byte character constant %s%.0ld%s isn't portable" /* _W4_ _W8_    */
                    , token, 0L, skip ? non_eval : NULL);
    }
    return  & ev;

range_err:
    if (wide)
        cerror( w_out_of_range, token, 0L, NULL);
    else
        cerror( c_out_of_range, token, 0L, NULL);
    ev.sign = VAL_ERROR;
    return  & ev;
}

static expr_t   eval_one(
    char **     seq_pp,         /* Address of pointer to sequence   */
                    /* eval_one() advances the pointer to sequence  */
    int     wide,                       /* Flag of wide-character   */
    int     mbits,              /* Number of bits of a wide-char    */
    int *   ucn8                            /* Flag of UCN-32 bits  */
)
/*
 * Called from eval_char() above to get a single character, single multi-
 * byte character or wide character (with or without \ escapes).
 * Returns the value of the character or -1L on error.
 */
{
    const char * const  out_of_range
        = "%s%ld bits can't represent escape sequence '%s'";    /* _E_ _W8_ */
    uexpr_t         value;
    int             erange = FALSE;
    char *          seq = *seq_pp;  /* Initial seq_pp for diagnostic*/
    const char *    cp;
    const char *    digits;
    unsigned        uc;
    unsigned        uc1;
    int             count;
    int             bits;
    size_t          wchar_max;

    uc = *(*seq_pp)++ & UCHARMAX;

    if (uc != '\\')                 /* Other than escape sequence   */
        return  (expr_t) uc;

    /* escape sequence  */
    uc1 = uc = *(*seq_pp)++ & UCHARMAX;
    switch (uc) {
    case 'a':
        return  '\a';
    case 'b':
        return  '\b';
    case 'f':
        return  '\f';
    case 'n':
        return  '\n';
    case 'r':
        return  '\r';
    case 't':
        return  '\t';
    case 'v':
        return  '\v';
    case 'x':                               /* '\xFF'               */
        digits = "0123456789abcdef";
        bits = 4;
        uc = *(*seq_pp)++ & UCHARMAX;
        break;
    case '0': case '1': case '2': case '3':
    case '4': case '5': case '6': case '7':
        digits = "01234567";
        bits = 3;
        break;
    case '\'':  case '"':   case '?':   case '\\':
        return  (expr_t) uc;
    default:
        goto  undefined;
    }

    wchar_max = (UCHARMAX << CHARBIT) | UCHARMAX;
    if (mbits == CHARBIT * 4) {
        wchar_max = (wchar_max << CHARBIT * 2) | wchar_max;
    }

    value = 0L;
    for (count = 0; ; ++count) {
        if (isupper( uc))
            uc = tolower( uc);
        if ((cp = strchr( digits, uc)) == NULL)
            break;
        if (count >= 3 && bits == 3)
            break;      /* Octal escape sequence at most 3 digits   */
        value = (value << bits) | (cp - digits);
        if (wchar_max < value)
        {
            if (! skip)
                goto  range_err;
            else
                erange = TRUE;
        }
        uc = *(*seq_pp)++ & UCHARMAX;
    }
    (*seq_pp)--;

    if (erange) {
        value &= wchar_max;
        goto  range_err;
    }

    if (count == 0 && bits == 4)            /* '\xnonsense'         */
        goto  undefined;
    if (! wide && (UCHARMAX < value)) {
        value &= UCHARMAX;
        goto  range_err;
    }
    return  (expr_t) value;

undefined:
    uc1 = **seq_pp;
    **seq_pp = EOS;                         /* For diagnostic       */
    if ((! skip && (warn_level & 1)) || (skip && (warn_level & 8)))
        cwarn(
    "Undefined escape sequence%s %.0ld'%s'"         /* _W1_ _W8_    */
                , skip ? non_eval : NULL, 0L, seq);
    **seq_pp = uc1;
    *seq_pp = seq + 1;
    return  (expr_t) '\\';              /* Returns the escape char  */

range_err:
    uc1 = **seq_pp;
    **seq_pp = EOS;                         /* For diagnostic       */
    if (wide) {
        if (! skip)
            cerror( out_of_range, NULL, (long) mbits, seq);
        else if (warn_level & 8)
            cwarn( out_of_range, non_eval, (long) mbits, seq);
    } else {
        if (! skip)
            cerror( out_of_range, NULL, (long) CHARBIT, seq);
        else if (warn_level & 8)
            cwarn( out_of_range, non_eval, (long) CHARBIT, seq);
    }

    **seq_pp = uc1;
    if (! skip)
        return  -1L;
    else
        return  (expr_t) value;
}

static VAL_SIGN *   eval_eval(
    VAL_SIGN *  valp,
    int         op
)
/*
 * One or two values are popped from the value stack and do arithmetic.
 * The result is pushed onto the value stack.
 * eval_eval() returns the new pointer to the top of the value stack.
 */
{
    const char * const  zero_div = "%sDivision by zero%.0ld%s"; /* _E_ _W8_ */
#if HAVE_LONG_LONG
    const char * const  neg_format =
"Negative value \"%" LL_FORM "d\" is converted to positive \"%" /* _W1_ _W8_*/
                    LL_FORM "u\"%%s";
#else
    const char * const  neg_format =
"Negative value \"%ld\" is converted to positive \"%lu\"%%s";   /* _W1_ _W8_*/
#endif
    expr_t  v1, v2;
    int     sign1, sign2;

    if (is_binary( op)) {
        v2 = (--valp)->val;
        sign2 = valp->sign;
    } else {
        v2 = 0L;                            /* Dummy    */
        sign2 = SIGNED;                     /* Dummy    */
    }
    v1 = (--valp)->val;
    sign1 = valp->sign;
    if (mcpp_debug & EXPRESSION) {
        mcpp_fprintf( DBG, "%s op %s", (is_binary( op)) ? "binary" : "unary"
                , opname[ op]);
        dump_val( ", v1 = ", valp);
        if (is_binary( op))
            dump_val( ", v2 = ", valp + 1);
        mcpp_fputc( '\n', DBG);
    }

    if ((sign1 == UNSIGNED || sign2 == UNSIGNED) && is_binary( op)
            && op != OP_ANA && op != OP_ORO && op != OP_SR && op != OP_SL) {
        if (((sign1 == SIGNED && v1 < 0L) || (sign2 == SIGNED && v2 < 0L)
                ) && ((! skip && (warn_level & 1))
                    || (skip && (warn_level & 8)))) {
            char    negate[(((sizeof (expr_t) * 8) / 3) + 1) * 2 + 50];
            expr_t  v3;

            v3 = (sign1 == SIGNED ? v1 : v2);
            sprintf( negate, neg_format, v3, v3);
            cwarn( negate, skip ? non_eval : NULL, 0L, NULL);
        }
        valp->sign = sign1 = sign2 = UNSIGNED;
    }
    if ((op == OP_SL || op == OP_SR)
            && ((! skip && (warn_level & 1)) || (skip && (warn_level & 8)))) {
        if (v2 < 0L || v2 >= sizeof (expr_t) * CHARBIT)
            cwarn( "Illegal shift count %.0s\"%ld\"%s"      /* _W1_ _W8_    */
                , NULL, (long) v2, skip ? non_eval : NULL);
#if HAVE_LONG_LONG
        else if (v2 >= sizeof (long) * CHARBIT
                && ((! skip && (warn_level & w_level))
                    || (skip && (warn_level & 8))))
            cwarn(
"Shift count %.0s\"%ld\" is larger than bit count of long%s"    /* _W1_ _W8_*/
                , NULL, (long) v2, skip ? non_eval : NULL);
#endif
    }
    if ((op == OP_DIV || op == OP_MOD) && v2 == 0L) {
        if (! skip) {
            cerror( zero_div, NULL, 0L, NULL);
            valp->sign = VAL_ERROR;
            return  valp;
        } else {
            if (warn_level & 8)
                cwarn( zero_div, NULL, 0L, non_eval);
            valp->sign = sign1;
            valp->val = (expr_t) EXPR_MAX;
            return  valp;
        }
    }

    if (sign1 == SIGNED)
        v1 = eval_signed( & valp, v1, v2, op);
    else
        v1 = eval_unsigned( & valp, (uexpr_t) v1, (uexpr_t) v2, op);

    if (valp->sign == VAL_ERROR)                /* Out of range */
        return  valp;

    switch (op) {
    case OP_NOT:    case OP_EQ:     case OP_NE:
    case OP_LT:     case OP_LE:     case OP_GT:     case OP_GE:
    case OP_ANA:    case OP_ORO:
        valp->sign = SIGNED;
        break;
    default:
        valp->sign = sign1;
        break;
    }
    valp->val = v1;
    return  valp;
}

static expr_t   eval_signed(
    VAL_SIGN ** valpp,
    expr_t      v1,
    expr_t      v2,
    int         op
)
/*
 * Apply the argument operator to the signed data.
 * OP_COL is a special case.
 */
{
    const char * const   illeg_op
        = "Bug: Illegal operator \"%s\" in eval_signed()";      /* _F_  */
    const char * const  not_portable
        = "\"%s\" of negative number isn't portable%.0ld%s";    /* _W1_ _W8_*/
    const char *    op_name = opname[ op];
    VAL_SIGN *      valp = *valpp;
    expr_t  val;
    int     chk;                /* Flag of overflow in long long    */

    switch (op) {
    case OP_EOE:
    case OP_PLU:                        break;
    case OP_NEG:
        chk = v1 && v1 == -v1;
        if (chk
#if HAVE_LONG_LONG
                || (v1 && (long) v1 == (long) -v1)
#endif 
            )
            overflow( op_name, valpp, chk);
        v1 = -v1;
        break;
    case OP_COM:    v1 = ~v1;           break;
    case OP_NOT:    v1 = !v1;           break;
    case OP_MUL:
        val = v1 * v2;
        chk = v1 && v2 && (val / v1 != v2 || val / v2 != v1);
        if (chk
#if HAVE_LONG_LONG
                || (v1 && v2
                    && ((long)val / (long)v1 != (long)v2
                        || (long)val / (long)v2 != (long)v1))
#endif
            )
            overflow( op_name, valpp, chk);
        v1 = val;
        break;
    case OP_DIV:
    case OP_MOD:
        /* Division by 0 has been already diagnosed by eval_eval(). */
        chk = -v1 == v1 && v2 == -1;
        if (chk             /* LONG_MIN / -1 on two's complement    */
#if HAVE_LONG_LONG
                || ((long)-v1 == (long)v1 && (long)v2 == (long)-1)
#endif
            )
            overflow( op_name, valpp, chk);
        else if ((v1 < 0L || v2 < 0L)
                && ((! skip && (warn_level & 1))
                    || (skip && (warn_level & 8))))
            cwarn( not_portable, op_name, 0L, skip ? non_eval : NULL);
        if (op == OP_DIV)
            v1 /= v2;
        else
            v1 %= v2;
        break;
    case OP_ADD:
        val = v1 + v2;
        chk = (v2 > 0L && v1 > val) || (v2 < 0L && v1 < val);
        if (chk
#if HAVE_LONG_LONG
                || ((((long)v2 > 0L && (long)v1 > (long)val)
                        || ((long)v2 < 0L && (long)v1 < (long)val)))
#endif
            )
            overflow( op_name, valpp, chk);
        v1 = val;
        break;
    case OP_SUB:
        val = v1 - v2;
        chk = (v2 > 0L && val > v1) || (v2 < 0L && val < v1);
        if (chk
#if HAVE_LONG_LONG
                || ((((long)v2 > 0L && (long)val > (long)v1)
                        || ((long)v2 < 0L && (long)val < (long)v1)))
#endif
            )
            overflow( op_name, valpp, chk);
        v1 = val;
        break;
    case OP_SL:     v1 <<= v2;          break;
    case OP_SR:
        if (v1 < 0L
                && ((!skip && (warn_level & 1))
                    || (skip && (warn_level & 8))))
            cwarn( not_portable, op_name, 0L, skip ? non_eval : NULL);
        v1 >>= v2;
        break;
    case OP_LT:     v1 = (v1 < v2);     break;
    case OP_LE:     v1 = (v1 <= v2);    break;
    case OP_GT:     v1 = (v1 > v2);     break;
    case OP_GE:     v1 = (v1 >= v2);    break;
    case OP_EQ:     v1 = (v1 == v2);    break;
    case OP_NE:     v1 = (v1 != v2);    break;
    case OP_AND:    v1 &= v2;           break;
    case OP_XOR:    v1 ^= v2;           break;
    case OP_OR:     v1 |= v2;           break;
    case OP_ANA:    v1 = (v1 && v2);    break;
    case OP_ORO:    v1 = (v1 || v2);    break;
    case OP_COL:
        /*
         * If v1 has the "true" value, v2 has the "false" value.
         * The top of the value stack has the test.
         */
        v1 = (--*valpp)->val ? v1 : v2;
        break;
    default:
        cfatal( illeg_op, op_name, 0L, NULL);
    }

    *valpp = valp;
    return  v1;
}

static expr_t   eval_unsigned(
    VAL_SIGN ** valpp,
    uexpr_t     v1u,
    uexpr_t     v2u,
    int         op
)
/*
 * Apply the argument operator to the unsigned data.
 * Called from eval_eval() only in Standard mode.
 */
{
    const char * const   illeg_op
        = "Bug: Illegal operator \"%s\" in eval_unsigned()";    /* _F_  */
    const char *    op_name = opname[ op];
    VAL_SIGN *      valp = *valpp;
    uexpr_t     v1 = 0;
    int     chk;        /* Flag of overflow in unsigned long long   */
    int     minus;      /* Big integer converted from signed long   */

    minus = (v1u > ULONGMAX || v2u > ULONGMAX);

    switch (op) {
    case OP_EOE:
    case OP_PLU:    v1 = v1u;           break;
    case OP_NEG:
        v1 = -v1u;
        if (v1u)
            overflow( op_name, valpp, TRUE);
        break;
    case OP_COM:    v1 = ~v1u;          break;
    case OP_NOT:    v1 = !v1u;          break;
    case OP_MUL:
        v1 = v1u * v2u;
        chk = v1u && v2u && (v1 / v2u != v1u || v1 / v1u != v2u);
        if (chk
#if HAVE_LONG_LONG
                || (! minus && v1 > ULONGMAX)
#endif
            )
            overflow( op_name, valpp, chk);
        break;
    case OP_DIV:
        /* Division by 0 has been already diagnosed by eval_eval().  */
        v1 = v1u / v2u;
        break;
    case OP_MOD:
        v1 = v1u % v2u;
        break;
    case OP_ADD:
        v1 = v1u + v2u;
        chk = v1 < v1u;
        if (chk
#if HAVE_LONG_LONG
                || (! minus && v1 > ULONGMAX)
#endif
            )
            overflow( op_name, valpp, chk);
        break;
    case OP_SUB:
        v1 = v1u - v2u;
        chk = v1 > v1u;
        if (chk
#if HAVE_LONG_LONG
                || (! minus && v1 > ULONGMAX)
#endif
            )
            overflow( op_name, valpp, chk);
        break;
    case OP_SL:     v1 = v1u << v2u;    break;
    case OP_SR:     v1 = v1u >> v2u;    break;
    case OP_LT:     v1 = (v1u < v2u);   break;
    case OP_LE:     v1 = (v1u <= v2u);  break;
    case OP_GT:     v1 = (v1u > v2u);   break;
    case OP_GE:     v1 = (v1u >= v2u);  break;
    case OP_EQ:     v1 = (v1u == v2u);  break;
    case OP_NE:     v1 = (v1u != v2u);  break;
    case OP_AND:    v1 = v1u & v2u;     break;
    case OP_XOR:    v1 = v1u ^ v2u;     break;
    case OP_OR:     v1 = v1u | v2u;     break;
    case OP_ANA:    v1 = (v1u && v2u);  break;
    case OP_ORO:    v1 = (v1u || v2u);  break;
    case OP_COL:    valp--;
        if (valp->val)
            v1 = v1u;
        else
            v1 = v2u;
        break;
    default:
        cfatal( illeg_op, op_name, 0L, NULL);
    }

    *valpp = valp;
    return  v1;
}

static void overflow(
    const char *    op_name,
    VAL_SIGN ** valpp,
    int         ll_overflow     /* Flag of overflow in long long    */
)
{
    const char * const  out_of_range
        = "Result of \"%s\" is out of range%.0ld%s";    /* _E_ _W1_ _W8_    */

#if HAVE_LONG_LONG
    if (! ll_overflow) {
        /* Overflow of long not in C99 mode */
        if ((! skip && (warn_level & w_level)) || (skip && (warn_level & 8)))
            cwarn( out_of_range, op_name, 0L, " of (unsigned) long");
    } else
#endif
    if (skip) {
        if (warn_level & 8)
            cwarn( out_of_range, op_name, 0L, non_eval);
        /* Else don't warn  */
    } else if ((*valpp)->sign == UNSIGNED) {/* Never overflow   */
        if (warn_level & 1)
            cwarn( out_of_range, op_name, 0L, NULL);
    } else {
        cerror( out_of_range, op_name, 0L, NULL);
        (*valpp)->sign = VAL_ERROR;
    }
}

static void dump_val(
    const char *        msg,
    const VAL_SIGN *    valp
)
/*
 * Dump a value by internal representation.
 */
{
#if HAVE_LONG_LONG
    const char * const  format
                = "%s(%ssigned long long) 0x%016" LL_FORM "x";
#else
    const char * const  format = "%s(%ssigned long) 0x%08lx";
#endif
    int     sign = valp->sign;

    mcpp_fprintf( DBG, format, msg, sign ? "" : "un", valp->val);
}

static void dump_stack(
    const OPTAB *       opstack,        /* Operator stack               */
    const OPTAB *       opp,            /* Pointer into operator stack  */
    const VAL_SIGN *    value,          /* Value stack                  */
    const VAL_SIGN *    valp            /* -> value vector              */
)
/*
 * Dump stacked operators and values.
 */
{
    if (opstack < opp)
        mcpp_fprintf( DBG, "Index op prec skip name -- op stack at %s"
                , infile->bptr);

    while (opstack < opp) {
        mcpp_fprintf( DBG, " [%2d] %2d %04o    %d %s\n", (int)(opp - opstack)
                , opp->op, opp->prec, opp->skip, opname[ (int)opp->op]);
        opp--;
    }

    while (value <= --valp) {
        mcpp_fprintf( DBG, "value[%d].val = ", (int)(valp - value));
        dump_val( "", valp);
        mcpp_fputc( '\n', DBG);
    }
}

