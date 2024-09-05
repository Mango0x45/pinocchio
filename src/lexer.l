%{
#include <err.h>
#include <stdbool.h>
#include <stdlib.h>

#include "parser.h"
#include "pinocchio.h"

#define YY_USER_INIT                                                           \
	do {                                                                       \
		if (!interactive)                                                      \
			BEGIN(linecont);                                                   \
	} while (false);

#define YY_USER_ACTION                                                         \
	do {                                                                       \
		yylloc.first_column = yylloc.last_column;                              \
		yylloc.first_line = yylloc.last_line;                                  \
		for (int i = 0; i < yyleng; i++) {                                     \
			if (yytext[i] == '\n') {                                           \
				yylloc.last_column = 1;                                        \
				yylloc.last_line++;                                            \
			} else                                                             \
				yylloc.last_column++;                                          \
		}                                                                      \
	} while (false);

extern bool interactive;
extern const char *current_file;
%}

%option nodefault
%option noinput nounput noyywrap

ws [ \t]

%x error
%s linecont

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

	/* Allow line-continuation when the newline is suffixed by a
	   backslash, but not in interactive mode!  Interactive usage should
	   have this functionality disabled so that you get instant feedback
	   after hitting the enter key. */
<linecont>\n{ws}*\\ ;

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
	user_error("%s:%d:%d: Unrecognized character ‘%c’",
		current_file, yylloc.first_line, yylloc.first_column, *yytext);
	BEGIN(error);
	return YYerror;
}
<error>.*|\n { BEGIN(0); }

%%
