
/*  A Bison parser, made from /vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y
    by GNU Bison version 1.28  */

#define YYBISON 1  /* Identify Bison output.  */

#define yyparse dice_yyparse
#define yylex dice_yylex
#define yyerror dice_yyerror
#define yylval dice_yylval
#define yychar dice_yychar
#define yydebug dice_yydebug
#define yynerrs dice_yynerrs
#define  lFLOAT  257
#define  INTEGER  258
#define  ARRAY  259
#define  NAME  260
#define  SIN  261
#define  COS  262
#define  TAN  263
#define  ACOS  264
#define  ASIN  265
#define  ATAN  266
#define  LOG  267
#define  EXP  268
#define  ABS_TOKEN  269
#define  SQRT  270
#define  WHERE  271
#define  EQEQ  272
#define  LT  273
#define  LE  274
#define  GT  275
#define  GE  276
#define  NE  277
#define  LTLT  278
#define  GTGT  279
#define  JOIN  280

#line 1 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"

/* Force the definition for Linux */
/* Possible bug in older Linux yacc */
#ifndef NOBISON
extern int yylex();
extern "C" {
  void yyerror( char *);
  int  yyparse( void );
}
#endif
#include <XdmfExpr.h>
#include <XdmfArray.h>
#include <XdmfHDF.h>
#include <math.h>

static XdmfArray *XdmfExprReturnValue;

class XdmfInt64Array : public XdmfArray {
public :
  XdmfInt64Array( XdmfInt64 Length ) {
    this->SetNumberType( XDMF_INT64_TYPE );
    this->SetNumberOfElements( Length );
    }
  XdmfInt64Array() {
    this->SetNumberType( XDMF_INT64_TYPE );
    this->SetNumberOfElements( 10 );
    };
};

#define ADD_XDMF_ARRAY_TO_SYMBOL( a ) \
  { \
  char  name[80]; \
  XdmfExprSymbol *sp; \
  sprintf( name, "XdmfArray_%X", ( XdmfLength)(a) ); \
  sp = XdmfExprSymbolLookup( name ); \
  sp->ClientData = (a); \
  }


#line 41 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
typedef union {
  double    DoubleValue;
  long    IntegerValue;
  void    *ArrayPointer;
  XdmfExprSymbol  *Symbol;
} YYSTYPE;
#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define  YYFINAL    111
#define  YYFLAG    -32768
#define  YYNTBASE  39

#define YYTRANSLATE(x) ((unsigned)(x) <= 280 ? yytranslate[x] : 43)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,    37,
    38,    29,    28,    31,    27,     2,    30,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,    36,    32,     2,
    33,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
    34,     2,    35,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     1,     3,     4,     5,     6,
     7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
    17,    18,    19,    20,    21,    22,    23,    24,    25,    26
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     2,     6,    10,    17,    24,    33,    42,    44,    46,
    50,    54,    58,    62,    66,    70,    74,    78,    82,    86,
    90,    94,    98,   102,   107,   114,   121,   128,   135,   142,
   149,   156,   161,   165,   170,   172,   176,   180,   184,   188,
   193,   197,   199
};

static const short yyrhs[] = {    40,
     0,     5,    33,    41,     0,     5,    33,    42,     0,     5,
    34,    41,    35,    33,    42,     0,     5,    34,    41,    35,
    33,    41,     0,     5,    34,     4,    36,     4,    35,    33,
    42,     0,     5,    34,     4,    36,     4,    35,    33,    41,
     0,    41,     0,    42,     0,    41,    28,    41,     0,    41,
    31,    41,     0,    41,    32,    41,     0,    41,    27,    41,
     0,    41,    29,    41,     0,    41,    30,    41,     0,    41,
    28,    42,     0,    41,    27,    42,     0,    41,    29,    42,
     0,    41,    30,    42,     0,    42,    28,    41,     0,    42,
    27,    41,     0,    42,    29,    41,     0,    42,    30,    41,
     0,     5,    34,    41,    35,     0,     5,    34,     4,    36,
     4,    35,     0,    17,    37,    41,    18,    42,    38,     0,
    17,    37,    41,    19,    42,    38,     0,    17,    37,    41,
    20,    42,    38,     0,    17,    37,    41,    21,    42,    38,
     0,    17,    37,    41,    22,    42,    38,     0,    17,    37,
    41,    23,    42,    38,     0,     6,    37,    41,    38,     0,
    37,    41,    38,     0,    26,    37,    41,    38,     0,     5,
     0,    42,    28,    42,     0,    42,    27,    42,     0,    42,
    29,    42,     0,    42,    30,    42,     0,     6,    37,    42,
    38,     0,    37,    42,    38,     0,     4,     0,     3,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
    65,    73,    81,    86,    98,   114,   125,   138,   145,   150,
   159,   236,   300,   309,   319,   328,   337,   346,   355,   364,
   373,   382,   391,   400,   410,   421,   442,   463,   484,   505,
   526,   547,   565,   569,   573,   590,   594,   598,   602,   606,
   614,   618,   622
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","FLOAT",
"INTEGER","ARRAY","NAME","SIN","COS","TAN","ACOS","ASIN","ATAN","LOG","EXP",
"ABS_TOKEN","SQRT","WHERE","EQEQ","LT","LE","GT","GE","NE","LTLT","GTGT","JOIN",
"'-'","'+'","'*'","'/'","','","';'","'='","'['","']'","':'","'('","')'","statemant_list",
"statement","ArrayExpression","ScalarExpression", NULL
};
#endif

static const short yyr1[] = {     0,
    39,    40,    40,    40,    40,    40,    40,    40,    40,    41,
    41,    41,    41,    41,    41,    41,    41,    41,    41,    41,
    41,    41,    41,    41,    41,    41,    41,    41,    41,    41,
    41,    41,    41,    41,    41,    42,    42,    42,    42,    42,
    42,    42,    42
};

static const short yyr2[] = {     0,
     1,     3,     3,     6,     6,     8,     8,     1,     1,     3,
     3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
     3,     3,     3,     4,     6,     6,     6,     6,     6,     6,
     6,     4,     3,     4,     1,     3,     3,     3,     3,     4,
     3,     1,     1
};

static const short yydefact[] = {     0,
    43,    42,    35,     0,     0,     0,     0,     1,     8,     9,
     0,     0,     0,     0,     0,    35,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     2,     3,
    42,     0,     0,     0,     0,     0,     0,     0,    33,    41,
    13,    17,    10,    16,    14,    18,    15,    19,    11,    12,
    21,    37,    20,    36,    22,    38,    23,    39,     0,    24,
    32,    40,     0,     0,     0,     0,     0,     0,    34,    42,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,    24,    25,     5,     4,     0,     0,     0,     0,
     0,     0,    26,    27,    28,    29,    30,    31,     0,     0,
     0,    37,    36,    38,    39,    25,     7,     6,     0,     0,
     0
};

static const short yydefgoto[] = {   109,
     8,     9,    33
};

static const short yypact[] = {    76,
-32768,-32768,   -24,   -19,    -5,     1,    80,-32768,   215,   -13,
    80,   104,    80,    80,    80,     6,    24,   -26,    80,    80,
    80,    80,    80,    80,    80,    80,    80,    80,   215,   -13,
    23,   197,   -13,   137,   105,   131,   149,   120,-32768,-32768,
    40,    -6,    40,    -6,    -1,-32768,    -1,-32768,-32768,-32768,
    40,    -6,    40,    -6,    -1,-32768,    -1,-32768,    57,    41,
-32768,-32768,     2,     2,     2,     2,     2,     2,-32768,    42,
   206,    53,    80,    39,     2,   144,   156,   161,   168,   173,
   180,    73,-32768,    61,   215,   -13,     2,   185,     2,     2,
     2,     2,-32768,-32768,-32768,-32768,-32768,-32768,    63,    80,
   192,    28,    28,-32768,-32768,-32768,   215,   -13,    96,    99,
-32768
};

static const short yypgoto[] = {-32768,
-32768,    22,     0
};


#define  YYLAST    247


static const short yytable[] = {    10,
    25,    26,    27,    28,     1,     2,    18,    74,    11,    12,
    30,    40,    35,    25,    26,    27,    28,    13,    42,    44,
    46,    48,    27,    28,    52,    54,    56,    58,    17,    23,
    24,    14,    29,    32,    34,    36,    37,    15,    75,    38,
    41,    43,    45,    47,    49,    50,    51,    53,    55,    57,
    19,    20,    21,    22,    23,    24,    91,    92,    59,    71,
    72,    39,    76,    77,    78,    79,    80,    81,    21,    22,
    23,    24,    86,    73,    88,    87,    99,    82,     1,     2,
     3,     4,     1,     2,    16,     4,   101,    84,   102,   103,
   104,   105,     5,   100,    85,   110,     5,   106,   111,   108,
     0,     6,     0,     0,     0,     6,     1,    31,    16,     4,
     0,     0,     7,     0,     0,     0,     7,     0,     0,     0,
     5,   107,     1,    70,    16,     4,     0,     0,     0,     6,
     0,    25,    26,    27,    28,     0,     5,     0,     0,     0,
     7,     0,    62,     0,     0,     6,     0,     0,    63,    64,
    65,    66,    67,    68,     0,     0,     7,    19,    20,    21,
    22,    23,    24,    19,    20,    21,    22,    23,    24,     0,
    89,    90,    91,    92,    61,    19,    20,    21,    22,    23,
    24,    93,    89,    90,    91,    92,    69,    89,    90,    91,
    92,     0,     0,    94,    89,    90,    91,    92,    95,    89,
    90,    91,    92,     0,     0,    96,    89,    90,    91,    92,
    97,    89,    90,    91,    92,     0,     0,    98,    89,    90,
    91,    92,    40,    19,    20,    21,    22,    23,    24,    62,
     0,    60,    19,    20,    21,    22,    23,    24,     0,     0,
    83,    19,    20,    21,    22,    23,    24
};

static const short yycheck[] = {     0,
    27,    28,    29,    30,     3,     4,     7,     6,    33,    34,
    11,    38,    13,    27,    28,    29,    30,    37,    19,    20,
    21,    22,    29,    30,    25,    26,    27,    28,     7,    31,
    32,    37,    11,    12,    13,    14,    15,    37,    37,    34,
    19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
    27,    28,    29,    30,    31,    32,    29,    30,    36,    38,
     4,    38,    63,    64,    65,    66,    67,    68,    29,    30,
    31,    32,    73,    33,    75,    37,     4,    36,     3,     4,
     5,     6,     3,     4,     5,     6,    87,    35,    89,    90,
    91,    92,    17,    33,    73,     0,    17,    35,     0,   100,
    -1,    26,    -1,    -1,    -1,    26,     3,     4,     5,     6,
    -1,    -1,    37,    -1,    -1,    -1,    37,    -1,    -1,    -1,
    17,   100,     3,     4,     5,     6,    -1,    -1,    -1,    26,
    -1,    27,    28,    29,    30,    -1,    17,    -1,    -1,    -1,
    37,    -1,    38,    -1,    -1,    26,    -1,    -1,    18,    19,
    20,    21,    22,    23,    -1,    -1,    37,    27,    28,    29,
    30,    31,    32,    27,    28,    29,    30,    31,    32,    -1,
    27,    28,    29,    30,    38,    27,    28,    29,    30,    31,
    32,    38,    27,    28,    29,    30,    38,    27,    28,    29,
    30,    -1,    -1,    38,    27,    28,    29,    30,    38,    27,
    28,    29,    30,    -1,    -1,    38,    27,    28,    29,    30,
    38,    27,    28,    29,    30,    -1,    -1,    38,    27,    28,
    29,    30,    38,    27,    28,    29,    30,    31,    32,    38,
    -1,    35,    27,    28,    29,    30,    31,    32,    -1,    -1,
    35,    27,    28,    29,    30,    31,    32
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/gnu/share/bison.simple"
/* This file comes from bison-1.28.  */

/* Skeleton output parser for bison,
   Copyright (C) 1984, 1989, 1990 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* This is the parser code that is written into each bison parser
  when the %semantic_parser declaration is not specified in the grammar.
  It was written by Richard Stallman by simplifying the hairy parser
  used when %semantic_parser is specified.  */

#ifndef YYSTACK_USE_ALLOCA
#ifdef alloca
#define YYSTACK_USE_ALLOCA
#else /* alloca not defined */
#ifdef __GNUC__
#define YYSTACK_USE_ALLOCA
#define alloca __builtin_alloca
#else /* not GNU C.  */
#if (!defined (__STDC__) && defined (sparc)) || defined (__sparc__) || defined (__sparc) || defined (__sgi) || (defined (__sun) && defined (__i386))
#define YYSTACK_USE_ALLOCA
#include <alloca.h>
#else /* not sparc */
/* We think this test detects Watcom and Microsoft C.  */
/* This used to test MSDOS, but that is a bad idea
   since that symbol is in the user namespace.  */
#if (defined (_MSDOS) || defined (_MSDOS_)) && !defined (__TURBOC__)
#if 0 /* No need for malloc.h, which pollutes the namespace;
   instead, just don't use alloca.  */
#include <malloc.h>
#endif
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
/* I don't know what this was needed for, but it pollutes the namespace.
   So I turned it off.   rms, 2 May 1997.  */
/* #include <malloc.h>  */
 #pragma alloca
#define YYSTACK_USE_ALLOCA
#else /* not MSDOS, or __TURBOC__, or _AIX */
#if 0
#ifdef __hpux /* haible@ilog.fr says this works for HPUX 9.05 and up,
     and on HPUX 10.  Eventually we can turn this on.  */
#define YYSTACK_USE_ALLOCA
#define alloca __builtin_alloca
#endif /* __hpux */
#endif
#endif /* not _AIX */
#endif /* not MSDOS, or __TURBOC__ */
#endif /* not sparc */
#endif /* not GNU C */
#endif /* alloca not defined */
#endif /* YYSTACK_USE_ALLOCA not defined */

#ifdef YYSTACK_USE_ALLOCA
#define YYSTACK_ALLOC alloca
#else
#define YYSTACK_ALLOC malloc
#endif

/* Note: there must be only one dollar sign in this file.
   It is replaced by the list of actions, each action
   as one case of the switch.  */

#define yyerrok    (yyerrstatus = 0)
#define yyclearin  (yychar = YYEMPTY)
#define YYEMPTY    -2
#define YYEOF    0
#define YYACCEPT  goto yyacceptlab
#define YYABORT   goto yyabortlab
#define YYERROR    goto yyerrlab1
/* Like YYERROR except do call yyerror.
   This remains here temporarily to ease the
   transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL    goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(token, value) \
do                \
  if (yychar == YYEMPTY && yylen == 1)        \
    { yychar = (token), yylval = (value);      \
      yychar1 = YYTRANSLATE (yychar);        \
      YYPOPSTACK;            \
      goto yybackup;            \
    }                \
  else                \
    { yyerror ("syntax error: cannot back up"); YYERROR; }  \
while (0)

#define YYTERROR  1
#define YYERRCODE  256

#ifndef YYPURE
#define YYLEX    yylex()
#endif

#ifdef YYPURE
#ifdef YYLSP_NEEDED
#ifdef YYLEX_PARAM
#define YYLEX    yylex(&yylval, &yylloc, YYLEX_PARAM)
#else
#define YYLEX    yylex(&yylval, &yylloc)
#endif
#else /* not YYLSP_NEEDED */
#ifdef YYLEX_PARAM
#define YYLEX    yylex(&yylval, YYLEX_PARAM)
#else
#define YYLEX    yylex(&yylval)
#endif
#endif /* not YYLSP_NEEDED */
#endif

/* If nonreentrant, generate the variables here */

#ifndef YYPURE

int  yychar;      /*  the lookahead symbol    */
YYSTYPE  yylval;      /*  the semantic value of the    */
        /*  lookahead symbol      */

#ifdef YYLSP_NEEDED
YYLTYPE yylloc;      /*  location data for the lookahead  */
        /*  symbol        */
#endif

int yynerrs;      /*  number of parse errors so far       */
#endif  /* not YYPURE */

#if YYDEBUG != 0
int yydebug;      /*  nonzero means print parse trace  */
/* Since this is uninitialized, it does not stop multiple parsers
   from coexisting.  */
#endif

/*  YYINITDEPTH indicates the initial size of the parser's stacks  */

#ifndef  YYINITDEPTH
#define YYINITDEPTH 200
#endif

/*  YYMAXDEPTH is the maximum size the stacks can grow to
    (effective only if the built-in stack extension method is used).  */

#if YYMAXDEPTH == 0
#undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif

/* Define __yy_memcpy.  Note that the size argument
   should be passed with type unsigned int, because that is what the non-GCC
   definitions require.  With GCC, __builtin_memcpy takes an arg
   of type size_t, but it can handle unsigned int.  */

#if __GNUC__ > 1    /* GNU C and GNU C++ define this.  */
#define __yy_memcpy(TO,FROM,COUNT)  __builtin_memcpy(TO,FROM,COUNT)
#else        /* not GNU C or C++ */
#ifndef __cplusplus

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (to, from, count)
     char *to;
     char *from;
     unsigned int count;
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#else /* __cplusplus */

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (char *to, char *from, unsigned int count)
{
  register char *t = to;
  register char *f = from;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#endif
#endif

#line 217 "/usr/gnu/share/bison.simple"

/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
#ifdef __cplusplus
#define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#else /* not __cplusplus */
#define YYPARSE_PARAM_ARG YYPARSE_PARAM
#define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
#endif /* not __cplusplus */
#else /* not YYPARSE_PARAM */
#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL
#endif /* not YYPARSE_PARAM */

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
#ifdef YYPARSE_PARAM
int yyparse (void *);
#else
int yyparse (void);
#endif
#endif

int
yyparse(YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  register int yystate;
  register int yyn;
  register short *yyssp;
  register YYSTYPE *yyvsp;
  int yyerrstatus;  /*  number of tokens to shift before error messages enabled */
  int yychar1 = 0;    /*  lookahead token as an internal (translated) token number */

  short  yyssa[YYINITDEPTH];  /*  the state stack      */
  YYSTYPE yyvsa[YYINITDEPTH];  /*  the semantic value stack    */

  short *yyss = yyssa;    /*  refer to the stacks thru separate pointers */
  YYSTYPE *yyvs = yyvsa;  /*  to allow yyoverflow to reallocate them elsewhere */

#ifdef YYLSP_NEEDED
  YYLTYPE yylsa[YYINITDEPTH];  /*  the location stack      */
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;

#define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
#define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  int yystacksize = YYINITDEPTH;
  int yyfree_stacks = 0;

#ifdef YYPURE
  int yychar;
  YYSTYPE yylval;
  int yynerrs;
#ifdef YYLSP_NEEDED
  YYLTYPE yylloc;
#endif
#endif

  YYSTYPE yyval;    /*  the variable used to return    */
        /*  semantic values from the action  */
        /*  routines        */

  int yylen;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Starting parse\n");
#endif

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;    /* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss - 1;
  yyvsp = yyvs;
#ifdef YYLSP_NEEDED
  yylsp = yyls;
#endif

/* Push a new state, which is found in  yystate  .  */
/* In all cases, when you get here, the value and location stacks
   have just been pushed. so pushing a state here evens the stacks.  */
yynewstate:

  *++yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Give user a chance to reallocate the stack */
      /* Use copies of these so that the &'s don't force the real ones into memory. */
      YYSTYPE *yyvs1 = yyvs;
      short *yyss1 = yyss;
#ifdef YYLSP_NEEDED
      YYLTYPE *yyls1 = yyls;
#endif

      /* Get the current used size of the three stacks, in elements.  */
      int size = yyssp - yyss + 1;

#ifdef yyoverflow
      /* Each stack pointer address is followed by the size of
   the data in use in that stack, in bytes.  */
#ifdef YYLSP_NEEDED
      /* This used to be a conditional around just the two extra args,
   but that might be undefined if yyoverflow is a macro.  */
      yyoverflow("parser stack overflow",
     &yyss1, size * sizeof (*yyssp),
     &yyvs1, size * sizeof (*yyvsp),
     &yyls1, size * sizeof (*yylsp),
     &yystacksize);
#else
      yyoverflow("parser stack overflow",
     &yyss1, size * sizeof (*yyssp),
     &yyvs1, size * sizeof (*yyvsp),
     &yystacksize);
#endif

      yyss = yyss1; yyvs = yyvs1;
#ifdef YYLSP_NEEDED
      yyls = yyls1;
#endif
#else /* no yyoverflow */
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
  {
    yyerror("parser stack overflow");
    if (yyfree_stacks)
      {
        free (yyss);
        free (yyvs);
#ifdef YYLSP_NEEDED
        free (yyls);
#endif
      }
    return 2;
  }
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
  yystacksize = YYMAXDEPTH;
#ifndef YYSTACK_USE_ALLOCA
      yyfree_stacks = 1;
#endif
      yyss = (short *) YYSTACK_ALLOC (yystacksize * sizeof (*yyssp));
      __yy_memcpy ((char *)yyss, (char *)yyss1,
       size * (unsigned int) sizeof (*yyssp));
      yyvs = (YYSTYPE *) YYSTACK_ALLOC (yystacksize * sizeof (*yyvsp));
      __yy_memcpy ((char *)yyvs, (char *)yyvs1,
       size * (unsigned int) sizeof (*yyvsp));
#ifdef YYLSP_NEEDED
      yyls = (YYLTYPE *) YYSTACK_ALLOC (yystacksize * sizeof (*yylsp));
      __yy_memcpy ((char *)yyls, (char *)yyls1,
       size * (unsigned int) sizeof (*yylsp));
#endif
#endif /* no yyoverflow */

      yyssp = yyss + size - 1;
      yyvsp = yyvs + size - 1;
#ifdef YYLSP_NEEDED
      yylsp = yyls + size - 1;
#endif

#if YYDEBUG != 0
      if (yydebug)
  fprintf(stderr, "Stack size increased to %d\n", yystacksize);
#endif

      if (yyssp >= yyss + yystacksize - 1)
  YYABORT;
    }

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Entering state %d\n", yystate);
#endif

  goto yybackup;
 yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
#if YYDEBUG != 0
      if (yydebug)
  fprintf(stderr, "Reading a token: ");
#endif
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)    /* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;    /* Don't call YYLEX any more */

#if YYDEBUG != 0
      if (yydebug)
  fprintf(stderr, "Now at end of input.\n");
#endif
    }
  else
    {
      yychar1 = YYTRANSLATE(yychar);

#if YYDEBUG != 0
      if (yydebug)
  {
    fprintf (stderr, "Next token is %d (%s", yychar, yytname[yychar1]);
    /* Give the individual parser a way to print the precise meaning
       of a token, for further debugging info.  */
#ifdef YYPRINT
    YYPRINT (stderr, yychar, yylval);
#endif
    fprintf (stderr, ")\n");
  }
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];

  /* yyn is what to do for this token type in this state.
     Negative => reduce, -yyn is rule number.
     Positive => shift, yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (yyn < 0)
    {
      if (yyn == YYFLAG)
  goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting token %d (%s), ", yychar, yytname[yychar1]);
#endif

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* count tokens shifted since error; after three, turn off error status.  */
  if (yyerrstatus) yyerrstatus--;

  yystate = yyn;
  goto yynewstate;

/* Do the default action for the current state.  */
yydefault:

  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;

/* Do a reduction.  yyn is the number of a rule to reduce with.  */
yyreduce:
  yylen = yyr2[yyn];
  if (yylen > 0)
    yyval = yyvsp[1-yylen]; /* implement default value of the action */

#if YYDEBUG != 0
  if (yydebug)
    {
      int i;

      fprintf (stderr, "Reducing via rule %d (line %d), ",
         yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (i = yyprhs[yyn]; yyrhs[i] > 0; i++)
  fprintf (stderr, "%s ", yytname[yyrhs[i]]);
      fprintf (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif


  switch (yyn) {

case 1:
#line 65 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
    /* 
    printf("Complete\n");
    printf("XdmfExprReturnValue Nelms = %d\n", XdmfExprReturnValue->GetNumberOfElements());
    */
    ;
    break;}
case 2:
#line 73 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
    XdmfArray *TempArray = ( XdmfArray *)yyvsp[0].ArrayPointer;

    /* printf("Setting %s from ArrayExpression\n", $1); */
    XdmfExprReturnValue = (XdmfArray *)yyvsp[-2].ArrayPointer;
    *XdmfExprReturnValue = *TempArray;
    delete TempArray;
    ;
    break;}
case 3:
#line 81 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
    /* printf("Setting %s from ScalarExpression\n", $1); */
    XdmfExprReturnValue = (XdmfArray *)yyvsp[-2].ArrayPointer;
    *XdmfExprReturnValue = yyvsp[0].DoubleValue;
    ;
    break;}
case 4:
#line 86 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray  *Array1 = ( XdmfArray *)yyvsp[-3].ArrayPointer;
      XdmfArray  *Result = ( XdmfArray *)yyvsp[-5].ArrayPointer;
      XdmfLength  i, index, Length = Array1->GetNumberOfElements();

      for( i = 0 ; i < Length ; i++ ){
        index = static_cast<XdmfLength>(Array1->GetValueAsFloat64( i ));
        Result->SetValueFromFloat64( index, yyvsp[0].DoubleValue );
        }
      delete Array1;
      XdmfExprReturnValue = Result;
    ;
    break;}
case 5:
#line 98 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray  *Array1 = ( XdmfArray *)yyvsp[-3].ArrayPointer;
      XdmfArray  *Array2 = ( XdmfArray *)yyvsp[0].ArrayPointer;
      XdmfArray  *Result = ( XdmfArray *)yyvsp[-5].ArrayPointer;
      XdmfFloat64  Value;
      XdmfLength  i, index, Length = Array1->GetNumberOfElements();

      for( i = 0 ; i < Length ; i++ ){
        index = static_cast<XdmfLength>(Array1->GetValueAsFloat64( i ));
        Value = Array2->GetValueAsFloat64( i );
        Result->SetValueFromFloat64( index, Value );
        }
      delete Array1;
      delete Array2;
      XdmfExprReturnValue = Result;
    ;
    break;}
case 6:
#line 114 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray *Range;

      /* printf("Array Range %d:%d = ScalarExpression \n", $3, $5);   */
      Range = (XdmfArray *)yyvsp[-7].ArrayPointer;
      XdmfExprReturnValue = Range->Reference( yyvsp[-5].IntegerValue, yyvsp[-3].IntegerValue ); /* This is a Reference */
      *XdmfExprReturnValue = yyvsp[0].DoubleValue;

      /* Now Point to the Entire Array */
      XdmfExprReturnValue = (XdmfArray *)yyvsp[-7].ArrayPointer;
      ;
    break;}
case 7:
#line 125 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray *TempArray = ( XdmfArray *)yyvsp[0].ArrayPointer;
      XdmfArray *Range;

      /* printf("Array Range %d:%d = ArrayExpression \n", $3, $5);   */
      Range = (XdmfArray *)yyvsp[-7].ArrayPointer;
      XdmfExprReturnValue = Range->Reference( yyvsp[-5].IntegerValue, yyvsp[-3].IntegerValue ); /* This is a Reference */
      *XdmfExprReturnValue = *TempArray;

      /* Now Point to the Entire Array */
      XdmfExprReturnValue = (XdmfArray *)yyvsp[-7].ArrayPointer;
      delete TempArray;
      ;
    break;}
case 8:
#line 138 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
    XdmfArray *TempArray = ( XdmfArray *)yyvsp[0].ArrayPointer;

    /* printf("Clone from ArrayExpression\n"); */
    XdmfExprReturnValue = TempArray;  
    /* printf("XdmfExprReturnValue Nelms = %d\n", XdmfExprReturnValue->GetNumberOfElements()); */
    ;
    break;}
case 9:
#line 145 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
    printf("Pointless !! Scalar = %g\n", yyvsp[0].DoubleValue);
    ;
    break;}
case 10:
#line 150 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray *Array1 = ( XdmfArray *)yyvsp[-2].ArrayPointer;
      XdmfArray *Array2 = ( XdmfArray *)yyvsp[0].ArrayPointer;

      /* printf("Array 0x%X + 0x%X\n", Array1, Array2); */
      *Array1 += *Array2;
      yyval.ArrayPointer = Array1;
      delete Array2;
      ;
    break;}
case 11:
#line 159 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      /* Interlace */
      XdmfArray *Array1 = ( XdmfArray *)yyvsp[-2].ArrayPointer;
      XdmfArray *Array2 = ( XdmfArray *)yyvsp[0].ArrayPointer;
      XdmfArray *NewArray = new XdmfArray();
      XdmfInt32 i, Rank1, Rank2;
      XdmfInt64 NewLength, Length1, Length2, IFactor, Lcd;
      XdmfInt64 Dimension1[ XDMF_MAX_DIMENSION ];
      XdmfInt64 Dimension2[ XDMF_MAX_DIMENSION ];
      XdmfInt64 Start[ XDMF_MAX_DIMENSION ];
      XdmfInt64 Stride[ XDMF_MAX_DIMENSION ];
      XdmfInt64 Count[ XDMF_MAX_DIMENSION ];
      XdmfInt64 NewDimension[ XDMF_MAX_DIMENSION ];

      /* printf("Array 0x%X , 0x%X\n", Array1, Array2); */
      
      Rank1 = Array1->GetShape( Dimension1 );
      Rank2 = Array2->GetShape( Dimension2 );
      if( Rank1 != Rank2 ){
        printf(" Interlace : Rank Mismatch !!\n");
        }
      NewArray->CopyType( Array1 );

      Length1 = Array1->GetNumberOfElements();
      Length2 = Array2->GetNumberOfElements();
      NewLength = Length1 + Length2;
      IFactor = Length1 / Length2;
      Lcd = Length1;
      if( Length2 < Length1 ){
        Lcd = Length2;
        }
      NewDimension[0] = Lcd;
      NewDimension[1] = NewLength / Lcd;
      NewArray->SetShape( 2, NewDimension );
      /*
      printf("Rank1 = %d Rank2 = %d\n", Rank1, Rank2 );
      printf("Array1 Size = %d\n", Array1->GetNumberOfElements() );
      printf("Array2 Size = %d\n", Array2->GetNumberOfElements() );
      printf("NewLength = %d\n", NewLength );
      printf("Lcd = %d\n", Lcd );
      printf("IFactor = %d\n", IFactor );
      printf("New Dims = %s\n", NewArray->GetShapeAsString() );
      */
      /* NewArray->Generate( -55.0,  -55.0 ); */
      /* Copy in Array 1 */
      Start[0] = 0; Start[1] = 0;
      Stride[0] = 1; Stride[1] = 1;
      Count[0] = Lcd; Count[1] = Length1 / Lcd;
      NewArray->SelectHyperSlab( Start, Stride, Count );
      Array1->SelectAll();
      /*
      printf("Copy in Array1 = %s\n", NewArray->GetHyperSlabAsString() );
      */
      CopyArray( Array1, NewArray );
      /* Copy in Array 2 */
      Start[0] = 0; Start[1] = Length1 / Lcd;
      Stride[0] = 1; Stride[1] = 1;
      Count[0] = Lcd; Count[1] = Length2 / Lcd;
      NewArray->SelectHyperSlab( Start, Stride, Count );
      Array2->SelectAll();
      /*
      printf("Copy in Array2 = %s\n", NewArray->GetHyperSlabAsString() );
      */
      CopyArray( Array2, NewArray );
      NewDimension[0] = Dimension1[0] + Dimension2[0];
      for( i = 1 ; i < Rank1 ; i++ ){
        NewDimension[i] = Dimension1[i];
        } 
      NewArray->Reform( Rank1, NewDimension );
      /*  
      printf("Result(%s) = %s\n", NewArray->GetShapeAsString(), NewArray->GetValues() );
      */
      yyval.ArrayPointer = NewArray;
      delete Array1;
      delete Array2;
      ;
    break;}
case 12:
#line 236 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      /* Interlace */
      XdmfArray *Array1 = ( XdmfArray *)yyvsp[-2].ArrayPointer;
      XdmfArray *Array2 = ( XdmfArray *)yyvsp[0].ArrayPointer;
      XdmfArray *NewArray = new XdmfArray();
      XdmfInt32 i, Rank1, Rank2;
      XdmfInt64 Dimension1[ XDMF_MAX_DIMENSION ];
      XdmfInt64 Dimension2[ XDMF_MAX_DIMENSION ];
      XdmfInt64 Start[ XDMF_MAX_DIMENSION ];
      XdmfInt64 Stride[ XDMF_MAX_DIMENSION ];
      XdmfInt64 Count[ XDMF_MAX_DIMENSION ];
      XdmfInt64 NewDimension[ XDMF_MAX_DIMENSION ];

       /* printf("Array 0x%X  << 0x%X\n", Array1, Array2); */
      
      Rank1 = Array1->GetShape( Dimension1 );
      Rank2 = Array2->GetShape( Dimension2 );
      if( Rank1 != Rank2 ){
        printf(" Cat : Rank Mismatch !!\n");
        }
      NewDimension[0] = Dimension1[0] + Dimension2[0];
      for( i = 1 ; i < Rank1 ; i++ ){
        NewDimension[i] = Dimension1[i];
        } 
      NewArray->CopyType( Array1 );
      NewArray->SetShape( Rank1, NewDimension );

      /*
      NewArray->Generate( -55.0,  -55.0 );
      */
      /* Copy in Array 1 */
      for( i = 0 ; i < Rank1 ; i++ ){
        Start[i] = 0;
        Stride[i] = 1;
        Count[i] = Dimension1[i];
        }
      NewArray->SelectHyperSlab( Start, Stride, Count );
      Array1->SelectAll();
      /*
      printf("Copy in Array1 = %s\n", NewArray->GetHyperSlabAsString() );
      */
      CopyArray( Array1, NewArray );
      /* Copy in Array 2 */
      Start[0] = Dimension1[0];
      Stride[0] = 1;
      Count[0] = Dimension2[0];
      for( i = 1 ; i < Rank1 ; i++ ){
        Start[i] = 0;
        Stride[i] = 1;
        Count[i] = Dimension1[i];
        }
      NewArray->SelectHyperSlab( Start, Stride, Count );
      Array2->SelectAll();
      /*
      printf("Copy in Array2 = %s\n", NewArray->GetHyperSlabAsString() );
      */
      CopyArray( Array2, NewArray );
      /*
      printf("Result(%s) = %s\n", NewArray->GetShapeAsString(), NewArray->GetValues() );
      */
      yyval.ArrayPointer = NewArray;
      delete Array1;
      delete Array2;
      ;
    break;}
case 13:
#line 300 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray *Array1 = ( XdmfArray *)yyvsp[-2].ArrayPointer;
      XdmfArray *Array2 = ( XdmfArray *)yyvsp[0].ArrayPointer;

      /* printf("Array 0x%X + 0x%X\n", Array1, Array2); */
      *Array1 -= *Array2;
      yyval.ArrayPointer = Array1;
      delete Array2;
      ;
    break;}
case 14:
#line 309 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray *Array1 = ( XdmfArray *)yyvsp[-2].ArrayPointer;
      XdmfArray *Array2 = ( XdmfArray *)yyvsp[0].ArrayPointer;

      /* printf("Array 0x%X * 0x%X\n", Array1, Array2); */
      *Array1 *= *Array2;
      yyval.ArrayPointer = Array1;
      delete Array2;
      /* printf("Array1 Nelms = %d\n", Array1->GetNumberOfElements()); */
      ;
    break;}
case 15:
#line 319 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray *Array1 = ( XdmfArray *)yyvsp[-2].ArrayPointer;
      XdmfArray *Array2 = ( XdmfArray *)yyvsp[0].ArrayPointer;

      /* printf("Array 0x%X + 0x%X\n", Array1, Array2); */
      *Array1 /= *Array2;
      yyval.ArrayPointer = Array1;
      delete Array2;
      ;
    break;}
case 16:
#line 328 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray *Array1 = ( XdmfArray *)yyvsp[-2].ArrayPointer;
      XdmfArray *Result;

      /* printf("Array + %g\n", $3); */
      Result  = Array1;
      *Result += yyvsp[0].DoubleValue;
      yyval.ArrayPointer = Result;
      ;
    break;}
case 17:
#line 337 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray *Array1 = ( XdmfArray *)yyvsp[-2].ArrayPointer;
      XdmfArray *Result;

      /* printf("Array - %g\n", $3); */
      Result  = Array1;
      *Result -= yyvsp[0].DoubleValue;
      yyval.ArrayPointer = Result;
      ;
    break;}
case 18:
#line 346 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray *Array1 = ( XdmfArray *)yyvsp[-2].ArrayPointer;
      XdmfArray *Result;

      /* printf("Array * %g\n", $3); */
      Result  = Array1;
      *Result *= yyvsp[0].DoubleValue;
      yyval.ArrayPointer = Result;
      ;
    break;}
case 19:
#line 355 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray *Array1 = ( XdmfArray *)yyvsp[-2].ArrayPointer;
      XdmfArray *Result;

      /* printf("Array / %g\n", $3); */
      Result  = Array1;
      *Result /= yyvsp[0].DoubleValue;
      yyval.ArrayPointer = Result;
      ;
    break;}
case 20:
#line 364 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray *Array1 = ( XdmfArray *)yyvsp[0].ArrayPointer;
      XdmfArray *Result;

      /* printf("Array + %g\n", $1); */
      Result  = Array1;
      *Result += yyvsp[-2].DoubleValue;
      yyval.ArrayPointer = Result;
      ;
    break;}
case 21:
#line 373 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray *Array1 = ( XdmfArray *)yyvsp[0].ArrayPointer;
      XdmfArray *Result;

      /* printf("Array - %g\n", $1); */
      Result  = Array1;
      *Result -= yyvsp[-2].DoubleValue;
      yyval.ArrayPointer = Result;
      ;
    break;}
case 22:
#line 382 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray *Array1 = ( XdmfArray *)yyvsp[0].ArrayPointer;
      XdmfArray *Result;

      /* printf("Array * %g\n", $1); */
      Result  = Array1;
      *Result *= yyvsp[-2].DoubleValue;
      yyval.ArrayPointer = Result;
      ;
    break;}
case 23:
#line 391 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray *Array1 = ( XdmfArray *)yyvsp[0].ArrayPointer;
      XdmfArray *Result;

      /* printf("Array / %g\n", $1); */
      Result  = Array1;
      *Result /= yyvsp[-2].DoubleValue;
      yyval.ArrayPointer = Result;
      ;
    break;}
case 24:
#line 400 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray  *Array1 = ( XdmfArray *)yyvsp[-3].ArrayPointer;
      XdmfArray  *Array2 = ( XdmfArray *)yyvsp[-1].ArrayPointer;
      XdmfArray  *Result;

      /* printf("ArrayExpression From Indexes\n"); */
      Result = Array1->Clone( Array2 );
      delete Array2;
      yyval.ArrayPointer = Result;
    ;
    break;}
case 25:
#line 410 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray *Array1 = ( XdmfArray *)yyvsp[-5].ArrayPointer;
      XdmfArray *Range, *Result;

      /* printf("ArrayExpression From Array Range %d:%d\n", $3, $5);   */
      Range = Array1->Reference( yyvsp[-3].IntegerValue, yyvsp[-1].IntegerValue ); /* This not a copy  */
  
      Result  = Range->Clone(); /* So Copy It */
      delete Array1;
      yyval.ArrayPointer = Result;
      ;
    break;}
case 26:
#line 421 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray  *Array1 = ( XdmfArray *)yyvsp[-3].ArrayPointer;
      XdmfLength  i, cntr = 0;
      XdmfLength  Length = Array1->GetNumberOfElements();
      XdmfInt64Array  *Index = new XdmfInt64Array( Length );
      XdmfFloat64  Value, SValue = yyvsp[-1].DoubleValue;

      for( i = 0 ; i < Length ; i++ ){
        Value = Array1->GetValueAsFloat64( i );
        if( Value == SValue ) {
          Index->SetValue( cntr++, i );
          }
        }  
      /* printf("Found %d Wheres\n", cntr ); */
      if( cntr == 0 ){
        yyerror("WHERE Function Length == 0");
        return( 0 );
        }
      Index->SetNumberOfElements( cntr );
      yyval.ArrayPointer = ( XdmfArray *)Index;
      ;
    break;}
case 27:
#line 442 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray  *Array1 = ( XdmfArray *)yyvsp[-3].ArrayPointer;
      XdmfLength  i, cntr = 0;
      XdmfLength  Length = Array1->GetNumberOfElements();
      XdmfInt64Array  *Index = new XdmfInt64Array( Length );
      XdmfFloat64  Value, SValue = yyvsp[-1].DoubleValue;

      for( i = 0 ; i < Length ; i++ ){
        Value = Array1->GetValueAsFloat64( i );
        if( Value < SValue ) {
          Index->SetValue( cntr++, i );
          }
        }  
      /* printf("Found %d Wheres\n", cntr ); */
      if( cntr == 0 ){
        yyerror("WHERE Function Length == 0");
        return( 0 );
        }
      Index->SetNumberOfElements( cntr );
      yyval.ArrayPointer = ( XdmfArray *)Index;
      ;
    break;}
case 28:
#line 463 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray  *Array1 = ( XdmfArray *)yyvsp[-3].ArrayPointer;
      XdmfLength  i, cntr = 0;
      XdmfLength  Length = Array1->GetNumberOfElements();
      XdmfInt64Array  *Index = new XdmfInt64Array( Length );
      XdmfFloat64  Value, SValue = yyvsp[-1].DoubleValue;

      for( i = 0 ; i < Length ; i++ ){
        Value = Array1->GetValueAsFloat64( i );
        if( Value <= SValue ) {
          Index->SetValue( cntr++, i );
          }
        }  
      /* printf("Found %d Wheres\n", cntr ); */
      if( cntr == 0 ){
        yyerror("WHERE Function Length == 0");
        return( 0 );
        }
      Index->SetNumberOfElements( cntr );
      yyval.ArrayPointer = ( XdmfArray *)Index;
      ;
    break;}
case 29:
#line 484 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray  *Array1 = ( XdmfArray *)yyvsp[-3].ArrayPointer;
      XdmfLength  i, cntr = 0;
      XdmfLength  Length = Array1->GetNumberOfElements();
      XdmfInt64Array  *Index = new XdmfInt64Array( Length );
      XdmfFloat64  Value, SValue = yyvsp[-1].DoubleValue;

      for( i = 0 ; i < Length ; i++ ){
        Value = Array1->GetValueAsFloat64( i );
        if( Value > SValue ) {
          Index->SetValue( cntr++, i );
          }
        }  
      /* printf("Found %d Wheres\n", cntr ); */
      if( cntr == 0 ){
        yyerror("WHERE Function Length == 0");
        return( 0 );
        }
      Index->SetNumberOfElements( cntr );
      yyval.ArrayPointer = ( XdmfArray *)Index;
      ;
    break;}
case 30:
#line 505 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray  *Array1 = ( XdmfArray *)yyvsp[-3].ArrayPointer;
      XdmfLength  i, cntr = 0;
      XdmfLength  Length = Array1->GetNumberOfElements();
      XdmfInt64Array  *Index = new XdmfInt64Array( Length );
      XdmfFloat64  Value, SValue = yyvsp[-1].DoubleValue;

      for( i = 0 ; i < Length ; i++ ){
        Value = Array1->GetValueAsFloat64( i );
        if( Value >= SValue ) {
          Index->SetValue( cntr++, i );
          }
        }  
      /* printf("Found %d Wheres\n", cntr ); */
      if( cntr == 0 ){
        yyerror("WHERE Function Length == 0");
        return( 0 );
        }
      Index->SetNumberOfElements( cntr );
      yyval.ArrayPointer = ( XdmfArray *)Index;
      ;
    break;}
case 31:
#line 526 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray  *Array1 = ( XdmfArray *)yyvsp[-3].ArrayPointer;
      XdmfLength  i, cntr = 0;
      XdmfLength  Length = Array1->GetNumberOfElements();
      XdmfInt64Array  *Index = new XdmfInt64Array( Length );
      XdmfFloat64  Value, SValue = yyvsp[-1].DoubleValue;

      for( i = 0 ; i < Length ; i++ ){
        Value = Array1->GetValueAsFloat64( i );
        if( Value != SValue ) {
          Index->SetValue( cntr++, i );
          }
        }  
      /* printf("Found %d Wheres\n", cntr ); */
      if( cntr == 0 ){
        yyerror("WHERE Function Length == 0");
        return( 0 );
        }
      Index->SetNumberOfElements( cntr );
      yyval.ArrayPointer = ( XdmfArray *)Index;
      ;
    break;}
case 32:
#line 547 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{

      if( yyvsp[-3].Symbol->DoubleFunctionPtr == NULL ){
        /* printf("Bad Function Ptr for %s\n", $1->Name ); */
        yyval.ArrayPointer = yyvsp[-1].ArrayPointer;
      } else {
        XdmfArray *Array1 = ( XdmfArray *)yyvsp[-1].ArrayPointer;
        XdmfFloat64  Value;
        XdmfLength  i, Length = Array1->GetNumberOfElements();

        /* printf("Function Call %s\n", $1->Name ); */
        for( i = 0 ; i < Length ; i++ ){
          Value = Array1->GetValueAsFloat64( i );
          Array1->SetValueFromFloat64( i, (yyvsp[-3].Symbol->DoubleFunctionPtr)( Value ) );
          }  
        yyval.ArrayPointer = Array1;
      }
      ;
    break;}
case 33:
#line 565 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      /* printf("( ArrayExpression )\n"); */
      yyval.ArrayPointer = yyvsp[-1].ArrayPointer;
      ;
    break;}
case 34:
#line 569 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      /* printf("( ArrayExpression )\n"); */
      yyval.ArrayPointer = yyvsp[-1].ArrayPointer;
      ;
    break;}
case 35:
#line 573 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      XdmfArray *Array1 = ( XdmfArray *)yyvsp[0].ArrayPointer;
      XdmfArray *Result;

      /* printf("ArrayExpression From Array\n"); */

      if ( Array1 == NULL ){
        /* Bomb */
        yyerror("NULL Array Pointer");
        return( 0 );
      } else {
        Result  = Array1->Clone();
        yyval.ArrayPointer = Result;
        }
      ;
    break;}
case 36:
#line 590 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      /* printf("Scalar +\n"); */
      yyval.DoubleValue = yyvsp[-2].DoubleValue + yyvsp[0].DoubleValue;
      ;
    break;}
case 37:
#line 594 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      /* printf("Scalar -\n"); */
      yyval.DoubleValue = yyvsp[-2].DoubleValue - yyvsp[0].DoubleValue;
      ;
    break;}
case 38:
#line 598 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      /* printf("Scalar *\n"); */
      yyval.DoubleValue = yyvsp[-2].DoubleValue * yyvsp[0].DoubleValue;
      ;
    break;}
case 39:
#line 602 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      /* printf("Scalar /\n"); */
      yyval.DoubleValue = yyvsp[-2].DoubleValue / yyvsp[0].DoubleValue;
      ;
    break;}
case 40:
#line 606 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      if( yyvsp[-3].Symbol->DoubleFunctionPtr == NULL ){
        /* printf("Bad Function Ptr for %s\n", $1->Name ); */
        yyval.DoubleValue = 0.0;
      } else {
        yyval.DoubleValue = (yyvsp[-3].Symbol->DoubleFunctionPtr)( yyvsp[-1].DoubleValue );
      }
      ;
    break;}
case 41:
#line 614 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      /* printf ("( ScalarExpression )\n"); */
      yyval.DoubleValue = yyvsp[-1].DoubleValue;
      ;
    break;}
case 42:
#line 618 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      /* printf ("ScalarExpression from INTEGER\n"); */
      yyval.DoubleValue = yyvsp[0].IntegerValue;
      ;
    break;}
case 43:
#line 622 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"
{
      /* printf ("ScalarExpression from FLOAT\n"); */
      yyval.DoubleValue = yyvsp[0].DoubleValue;
      ;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 543 "/usr/gnu/share/bison.simple"

  yyvsp -= yylen;
  yyssp -= yylen;
#ifdef YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "state stack now");
      while (ssp1 != yyssp)
  fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;

#ifdef YYLSP_NEEDED
  yylsp++;
  if (yylen == 0)
    {
      yylsp->first_line = yylloc.first_line;
      yylsp->first_column = yylloc.first_column;
      yylsp->last_line = (yylsp-1)->last_line;
      yylsp->last_column = (yylsp-1)->last_column;
      yylsp->text = 0;
    }
  else
    {
      yylsp->last_line = (yylsp+yylen-1)->last_line;
      yylsp->last_column = (yylsp+yylen-1)->last_column;
    }
#endif

  /* Now "shift" the result of the reduction.
     Determine what state that goes to,
     based on the state we popped back to
     and the rule number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;

yyerrlab:   /* here on detecting error */

  if (! yyerrstatus)
    /* If not already recovering from an error, report this error.  */
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
  {
    int size = 0;
    char *msg;
    int x, count;

    count = 0;
    /* Start X at -yyn if nec to avoid negative indexes in yycheck.  */
    for (x = (yyn < 0 ? -yyn : 0);
         x < (sizeof(yytname) / sizeof(char *)); x++)
      if (yycheck[x + yyn] == x)
        size += strlen(yytname[x]) + 15, count++;
    msg = (char *) malloc(size + 15);
    if (msg != 0)
      {
        strcpy(msg, "parse error");

        if (count < 5)
    {
      count = 0;
      for (x = (yyn < 0 ? -yyn : 0);
           x < (sizeof(yytname) / sizeof(char *)); x++)
        if (yycheck[x + yyn] == x)
          {
      strcat(msg, count == 0 ? ", expecting `" : " or `");
      strcat(msg, yytname[x]);
      strcat(msg, "'");
      count++;
          }
    }
        yyerror(msg);
        free(msg);
      }
    else
      yyerror ("parse error; also virtual memory exceeded");
  }
      else
#endif /* YYERROR_VERBOSE */
  yyerror("parse error");
    }

  goto yyerrlab1;
yyerrlab1:   /* here on error raised explicitly by an action */

  if (yyerrstatus == 3)
    {
      /* if just tried and failed to reuse lookahead token after an error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
  YYABORT;

#if YYDEBUG != 0
      if (yydebug)
  fprintf(stderr, "Discarding token %d (%s).\n", yychar, yytname[yychar1]);
#endif

      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token
     after shifting the error token.  */

  yyerrstatus = 3;    /* Each real token shifted decrements this */

  goto yyerrhandle;

yyerrdefault:  /* current state does not do anything special for the error token. */

#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */
  yyn = yydefact[yystate];  /* If its default is to accept any token, ok.  Otherwise pop it.*/
  if (yyn) goto yydefault;
#endif

yyerrpop:   /* pop the current state because it cannot handle the error token */

  if (yyssp == yyss) YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#ifdef YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "Error: state stack now");
      while (ssp1 != yyssp)
  fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

yyerrhandle:

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
  goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting error token, ");
#endif

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;

 yyacceptlab:
  /* YYACCEPT comes here.  */
  if (yyfree_stacks)
    {
      free (yyss);
      free (yyvs);
#ifdef YYLSP_NEEDED
      free (yyls);
#endif
    }
  return 0;

 yyabortlab:
  /* YYABORT comes here.  */
  if (yyfree_stacks)
    {
      free (yyss);
      free (yyvs);
#ifdef YYLSP_NEEDED
      free (yyls);
#endif
    }
  return 1;
}
#line 629 "/vis/clarke/IceDevel02/Base/Xdmf/libsrc/XdmfExpr.y"


/* extern  FILE  *yyin, *yyout; */

#ifdef __cplusplus
/**/
extern "C" {
/**/
#endif

static  char  InputBuffer[ 512 ];
static  int  InputBufferPtr = 0, InputBufferEnd = 0;
static  char  OutputBuffer[ 512 ];
static  int  OutputBufferPtr = 0;
//static  int  OutputBufferEnd = 511;

int
dice_yywrap( void ) {

return 1;
}

void
dice_yyerror( char *string ) {
fprintf(stderr, "XdmfExpr : %s \n", string);
}

int
XdmfExprFlexInput( char *buf, int maxlen ) {
  (void)maxlen;
if ( InputBufferPtr < InputBufferEnd ){
  buf[0] = InputBuffer[ InputBufferPtr++ ];
  return(1);
} else {
  buf[0] = '\n';
  return( 0 );
  }
}

int
XdmfExprInput( void ){

if ( InputBufferPtr < InputBufferEnd ){
  return( InputBuffer[ InputBufferPtr++ ] );
} else {
  return '\n';
  }
}

void
XdmfExprUnput( int c ) {
if( InputBufferPtr > 0 ){
  InputBufferPtr--;
  InputBuffer[ InputBufferPtr ] = c;
  }
}

void
XdmfExprOutput( int c ) {
  /* printf("XdmfExprOutput Called\n"); */
  OutputBuffer[ OutputBufferPtr++ ] = c;
  OutputBuffer[ OutputBufferPtr ] = '\0';
  }

XdmfExprSymbol
*XdmfExprSymbolLookup( char *Name ){

static XdmfExprSymbol *Table = NULL;

XdmfExprSymbol  *Last = NULL, *Item = Table;

if( Name == NULL ) {
  /* Table Check  */
  return( Table );
  }

while( Item != NULL ) {
  if( strcmp( Item->Name, Name ) == 0 ) {
    /* printf("Found Symbol %s\n", Name ); */
    return( Item );
    }
  Last = Item;
  Item = Item->Next;
}
/* Not Found : Create New One */
Item = ( XdmfExprSymbol *)calloc( 1, sizeof( XdmfExprSymbol ));
Item->Next = NULL;
Item->Name = strdup( Name );
Item->ClientData = NULL;
Item->DoubleValue = 0;
Item->DoubleFunctionPtr = NULL;
if( Table == NULL ) {
  Table = Item;
  }
if( Last != NULL ){
  Last->Next = Item;
  }
/* printf("New Symbol for %s\n", Name ); */
return( Item );
}

#ifdef __cplusplus
/**/
}
/**/
#endif

XdmfArray *
XdmfExprParse( char *string ){

XdmfExprSymbol  *Item;
XdmfLength  CurrentTime;
XdmfLength  TimeOfCreation;
XdmfArray  *ap;

/* Build the Symbol Table if Necessary */
Item = XdmfExprSymbolLookup( NULL );
if( Item == NULL ){
  /* printf("Creating Symbol Table\n"); */
  Item = XdmfExprSymbolLookup( "cos" );
  Item->DoubleFunctionPtr = cos;
  Item = XdmfExprSymbolLookup( "sin" );
  Item->DoubleFunctionPtr = sin;
  Item = XdmfExprSymbolLookup( "exp" );
  Item->DoubleFunctionPtr = exp;
  Item = XdmfExprSymbolLookup( "tan" );
  Item->DoubleFunctionPtr = tan;
  Item = XdmfExprSymbolLookup( "acos" );
  Item->DoubleFunctionPtr = acos;
  Item = XdmfExprSymbolLookup( "asin" );
  Item->DoubleFunctionPtr = asin;
  Item = XdmfExprSymbolLookup( "atan" );
  Item->DoubleFunctionPtr = atan;
  Item = XdmfExprSymbolLookup( "log" );
  Item->DoubleFunctionPtr = log;
  Item = XdmfExprSymbolLookup( "sqrt" );
  Item->DoubleFunctionPtr = sqrt;
  }
/* Print Symbol Table */
Item = XdmfExprSymbolLookup( NULL );
while( Item != NULL ) {
  if( Item->ClientData != NULL ){
    /* printf("Found Symbol %s\n", Item->Name ); */
    }
  Item = Item->Next;
  }
strcpy( InputBuffer, string );
InputBufferEnd = strlen( InputBuffer );
InputBufferPtr = OutputBufferPtr = 0;
XdmfExprReturnValue = NULL;
/* printf("XdmfExprParse Scanning <%s>\n", InputBuffer); */
CurrentTime = GetCurrentArrayTime();
if ( yyparse() != 0 ){
  /* Error */
  XdmfExprReturnValue = NULL;
  }
/* Remove All Arrays Older than when we started */
/* printf("Cleaning up Temparary Arrays\n"); */
while( ( ap = GetNextOlderArray( CurrentTime, &TimeOfCreation ) ) != NULL ){
  /* Don't remove the return value */
  if( ap != XdmfExprReturnValue ){
    /* printf("Removing Temporary Array\n"); */
    delete ap;
    }
  CurrentTime = TimeOfCreation;
  }
return( XdmfExprReturnValue );
}

