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
		yylloc.first_line   = yylloc.last_line;                                \
		yylloc.first_column = yylloc.last_column;                              \
		yylloc.last_column += yyleng;                                          \
	} while (false);

#define LOCNL                                                                  \
	do {                                                                       \
		yylloc.last_line++;                                                    \
		yylloc.last_column = 1;                                                \
	} while (false)                                                            \

extern bool interactive, utf8;
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

\n {
	LOCNL;
	return EOL;
}

	/* Allow line-continuation when the newline is suffixed by a
	   backslash, but not in interactive mode!  Interactive usage should
	   have this functionality disabled so that you get instant feedback
	   after hitting the enter key. */
<linecont>\n{ws}*\\ {
	LOCNL;
	yylloc.last_column = yyleng;
}

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
	char ch = *yytext;

	static const char *quotes[][2] = {
		{"`", "'"},
		{"‘", "’"},
	};

	const char *lquot = quotes[utf8][0],
	           *rquot = quotes[utf8][1];

	if (ch == '&') {
		user_error("%s:%d:%d: Unrecognized character %s&%s, did you mean %s&&%s?",
			current_file, yylloc.first_line, yylloc.first_column,
			lquot, rquot, lquot, rquot);
	} else if (ch == '^') {
		user_error("%s:%d:%d: Unrecognized character %s^%s, did you mean %s~%s?",
			current_file, yylloc.first_line, yylloc.first_column,
			lquot, rquot, lquot, rquot);
	} else {
		user_error("%s:%d:%d: Unrecognized character %s%c%s",
			current_file, yylloc.first_line, yylloc.first_column,
			lquot, ch, rquot);
	}
	BEGIN(error);
	return YYerror;
}

<error>.*   {        BEGIN(0); }
<error>.*\n { LOCNL; BEGIN(0); }

%%
