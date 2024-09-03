%option noinput
%option nounput
%option noyywrap

%{
#include <err.h>

#include "parser.h"

#define YY_USER_ACTION yylloc.first_line = yylloc.last_line = yylineno;
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
\n      { return EOL;   } /* Only eat one NL for better interactive usage */
[a-zA-Z] {
	yylval.ch = *yytext;
	return IDENT;
}

[ \t]+ ;

	/* TODO: Swap ‘-’ for name of the input file */
. { errx(1, "%s:%d: Unrecognized character ‘%c’", "-", yylineno, *yytext); }

%%
