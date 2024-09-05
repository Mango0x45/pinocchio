%{
#include <ctype.h>
#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "lexer.h"
#include "parser.h"
#include "wrapper.h"

static ast_t mkunop(int, ast_t);
static ast_t mkbinop(int, ast_t, ast_t);
static asts_t pushast(asts_t, ast_t);
static void yyerror(const char *);

extern const char *current_file;
%}

%code requires {
#include "pinocchio.h"
}

%union {
	char   ch;
	ast_t  ast;
	asts_t asts;
}

%define parse.error verbose
%locations

/* Very important that NOT is the first token declared!  Code depends on it. */

%start input
%token NOT AND OR XOR IMPL
%token EQUIV OPAR CPAR EOL
%token<ch> IDENT
%type<ast> expr
%type<asts> line exprs

%left OR
%left AND
%left XOR
%left IMPL EQUIV
%precedence NOT

%%

input:
	  %empty
	| input line {
		if ($2.len > 0)
			astprocess($2);
	}
	;

line:
	  EOL       { $$.len = 0; }
	| exprs eol { $$ = $1; }
	;

exprs:
	  expr {
		$$.len = 1;
		$$.cap = 8;
		$$.buf = xmalloc(sizeof(*$$.buf) * $$.cap);
		$$.buf[0] = $1;
	}
	| exprs      '|' expr { $$ = pushast($1, $3); }
	| exprs cont '|' expr { $$ = pushast($1, $4); }
	| exprs '|' cont expr { $$ = pushast($1, $4); }
	;

expr:
	  IDENT {
		$$.eqn = xmalloc(sizeof(eqn_t));
		$$.eqn->type = IDENT;
		$$.eqn->ch = $1;
		$$.vars = UINT64_C(1) << (islower($1) ? $1-'a'+26 : $1-'A');
	}
	| NOT expr        { $$ = mkunop(NOT,  $2);       }
	| OPAR expr CPAR  { $$ = mkunop(OPAR, $2);       }
	| expr AND   expr { $$ = mkbinop(AND,   $1, $3); }
	| expr OR    expr { $$ = mkbinop(OR,    $1, $3); }
	| expr XOR   expr { $$ = mkbinop(XOR,   $1, $3); }
	| expr IMPL  expr { $$ = mkbinop(IMPL,  $1, $3); }
	| expr EQUIV expr { $$ = mkbinop(EQUIV, $1, $3); }
	;

cont: EOL '\\';
eol: EOL | YYEOF;

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

asts_t
pushast(asts_t as, ast_t a)
{
	if (as.len == as.cap) {
		as.cap *= 2;
		as.buf = xrealloc(as.buf, sizeof(*as.buf) * as.cap);
	}
	as.buf[as.len++] = a;
	return as;
}

void
yyerror(const char *s)
{
	user_error("%s:%d: %s", current_file, yylloc.first_line, s);
}
