%{
#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include "lexer.h"
#include "parser.h"

static ast_t mkunop(int, ast_t);
static ast_t mkbinop(int, ast_t, ast_t);
static void *xmalloc(size_t);
static void yyerror(const char *);

extern const char *current_file;
%}

%code requires {
#include "pinocchio.h"
}

%union {
	char  ch;
	ast_t ast;
}

%define parse.error verbose
%locations

/* Very important that NOT is the first token declared!  Code depends on it. */

%start input
%type  <ast> line exp
%token <ast> NOT AND OR XOR IMPL EQUIV OPAR CPAR
%token <ch>  IDENT
%token EOL

%left OR
%left AND
%left XOR
%left IMPL EQUIV
%nonassoc NOT

%%

input:
	  %empty
	| input line {
		if ($2.eqn != NULL)
			astprocess($2);
	}
	;

line:
	      EOL { $$.eqn = NULL; }
	| exp eol { $$ = $1; }
	;

eol: EOL | YYEOF;

exp:
	  IDENT {
		$$.eqn = xmalloc(sizeof(eqn_t));
		$$.eqn->type = IDENT;
		$$.eqn->ch = $1;
		$$.vars = 1 << (islower($1) ? $1-'a'+26 : $1-'A');
	}
	| NOT exp       { $$ = mkunop(NOT,  $2);       }
	| OPAR exp CPAR { $$ = mkunop(OPAR, $2);       }
	| exp AND   exp { $$ = mkbinop(AND,   $1, $3); }
	| exp OR    exp { $$ = mkbinop(OR,    $1, $3); }
	| exp XOR   exp { $$ = mkbinop(XOR,   $1, $3); }
	| exp IMPL  exp { $$ = mkbinop(IMPL,  $1, $3); }
	| exp EQUIV exp { $$ = mkbinop(EQUIV, $1, $3); }
	;

%%

ast_t
mkunop(int op, ast_t rhs)
{
	ast_t a = {
		.eqn  = xmalloc(sizeof(eqn_t)),
		.vars = rhs.vars,
	};
	a.eqn->type = op;
	a.eqn->rhs = rhs.eqn;
	return a;
}

ast_t
mkbinop(int op, ast_t lhs, ast_t rhs)
{
	ast_t a = {
		.eqn  = xmalloc(sizeof(eqn_t)),
		.vars = lhs.vars | rhs.vars,
	};
	a.eqn->type = op;
	a.eqn->lhs = lhs.eqn;
	a.eqn->rhs = rhs.eqn;
	return a;
}

void *
xmalloc(size_t n)
{
	void *p = malloc(n);
	if (p == NULL)
		err(1, "malloc");
	return p;
}

void
yyerror(const char *s)
{
	user_error("%s:%d: %s", current_file, yylloc.first_line, s);
}
