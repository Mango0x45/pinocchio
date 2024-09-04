%option noinput
%option nounput
%option noyywrap
%option yylineno

%{
#include <err.h>
#include <stdlib.h>

#include "parser.h"
#include "pinocchio.h"

#define YY_USER_ACTION yylloc.first_line = yylloc.last_line = yylineno;

extern const char *current_file;
%}

%%

¬|!     { return NOT;   }
∧|&&    { return AND;   }
∨|\|\|  { return OR;    }
⊻|⊕|XOR { return XOR;   }
⇒|=>    { return IMPL;  }
\<=>|⇔  { return EQUIV; }
\(      { return OPAR;  }
\)      { return CPAR;  }
\n      { return EOL;   }
[a-zA-Z] {
	yylval.ch = *yytext;
	return IDENT;
}

[ \t]+ ;

. {
	user_error("%s:%d: Unrecognized character ‘%c’",
		current_file, yylineno, *yytext);
}

%%
