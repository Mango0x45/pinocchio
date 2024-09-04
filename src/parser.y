%{
#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include "lexer.h"
#include "parser.h"

static ast_t astmerge(int, ast_t, ast_t);
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

input
	: %empty
	| input line { astprocess($2); }
	;

line
	:     EOL { $$.eqn = NULL; }
	| exp EOL { $$ = $1; }
	;

exp
	: IDENT {
		$$.eqn = xmalloc(sizeof(eqn_t));
		$$.eqn->type = IDENT;
		$$.eqn->ch  = $1;
		$$.vars = 1 << (islower($1) ? $1-'a'+26 : $1-'A');
	}
	| NOT exp {
		eqn_t *node = xmalloc(sizeof(eqn_t));
		node->type = NOT;
		node->rhs  = $2.eqn;
		$$.eqn = node;
		$$.vars = $2.vars;
	}
	| OPAR exp CPAR {
		eqn_t *node = xmalloc(sizeof(eqn_t));
		node->type = OPAR;
		node->rhs  = $2.eqn;
		$$.eqn = node;
		$$.vars = $2.vars;
	}
	| exp AND   exp { $$ = astmerge(AND,   $1, $3); }
	| exp OR    exp { $$ = astmerge(OR,    $1, $3); }
	| exp XOR   exp { $$ = astmerge(XOR,   $1, $3); }
	| exp IMPL  exp { $$ = astmerge(IMPL,  $1, $3); }
	| exp EQUIV exp { $$ = astmerge(EQUIV, $1, $3); }
	;

%%

ast_t
astmerge(int op, ast_t lhs, ast_t rhs)
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
