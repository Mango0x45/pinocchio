%{
#include <err.h>
#include <stdlib.h>

#include "parser.h"
#include "pinocchio.h"

#define YY_USER_ACTION yylloc.first_line = yylloc.last_line = yylineno;

extern const char *current_file;
%}

%option yylineno
%option nodefault
%option noinput nounput noyywrap

ws [ \t]

%x error

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

	/* Throw an error on an invalid token.  When in interactive mode we
	   should slurp up all data on the current line after reporting the
	   error so that lexing/parsing doesn’t continue right after the
	   offending token but instead on the next line typed by the user. */
. {
	user_error("%s:%d: Unrecognized character ‘%c’",
		current_file, yylineno, *yytext);
	BEGIN(error);
	return YYerror;
}
<error>.*|\n { BEGIN(0); }

%%
