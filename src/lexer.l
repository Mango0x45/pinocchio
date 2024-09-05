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

ws [ \t]

%%

¬|!    { return NOT;   }
∧|&&   { return AND;   }
∨|\|\| { return OR;    }
⊻|⊕|~  { return XOR;   }
⇒|=>   { return IMPL;  }
\<=>|⇔ { return EQUIV; }
\(     { return OPAR;  }
\)     { return CPAR;  }
\|     { return '|';   }
\\     { return '\\';  }
\n     { return EOL;   }

	/* Allow line-continuation when the newline is suffixed by a backslash */
\n{ws}*\\ ;

[a-zA-Z] {
	yylval.ch = *yytext;
	return IDENT;
}

{ws}+ ;

. {
	user_error("%s:%d: Unrecognized character ‘%c’",
		current_file, yylineno, *yytext);
}

%%
