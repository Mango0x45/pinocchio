#include <ctype.h>
#include <err.h>
#include <getopt.h>
#include <langinfo.h>
#include <libgen.h>
#include <locale.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lexer.h"
#include "parser.h"

#define MAXVARS (26 * 2)

#ifndef __has_builtin
#	define __has_builtin(x) (0)
#endif

static int rv;
static bool interactive, utf8;

bool lflag;
const char *current_file;

static bool eqnsolve(eqn_t *, uint64_t, uint64_t);
static int  eqnprint(eqn_t *);
static void eqnfree(eqn_t *);

static uint64_t
popcnt(uint64_t n)
{
#if __has_builtin(__builtin_popcount)
	return __builtin_popcount(n);
#else
    uint64_t c;
    for (c = 0; n > 0; n &= n - 1)
		c++; 
    return c; 
#endif
}

int
main(int argc, char **argv)
{
	argv[0] = basename(argv[0]);
	setlocale(LC_ALL, "");
	utf8 = strcmp(nl_langinfo(CODESET), "UTF-8") == 0;
	interactive = isatty(STDIN_FILENO);

	int opt;
	static struct option longopts[] = {
		{"latex", no_argument, 0, 'l'},
		{0},
	};

	while ((opt = getopt_long(argc, argv, "l", longopts, NULL)) != -1) {
		switch (opt) {
		case 'l':
			lflag = true;
			break;
		default:
			fprintf(stderr, "Usage: %s [-l] [file ...]\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		current_file = "-";
		for (;;) {
			if (yyparse() == 0)
				break;
			rv = EXIT_FAILURE;
		}
	} else for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-") == 0)
			yyin = stdin;
		else if ((yyin = fopen(argv[i], "r")) == NULL) {
			warn("fopen: %s", argv[i]);
			rv = EXIT_FAILURE;
			continue;
		}

		current_file = argv[i];
		yyparse();
		fclose(yyin);
	}

	return rv;
}

void
astprocess_cli(ast_t a)
{
	enum {
		TBLVBAR,
		TBLHBAR,
		TBLCROS,
	};

	static const char *symbols_utf8[] = {
		[TBLVBAR] = "│",
		[TBLHBAR] = "─",
		[TBLCROS] = "┼",
	};
	static const char *symbols_ascii[] = {
		[TBLVBAR] = "|",
		[TBLHBAR] = "-",
		[TBLCROS] = "+",
	};

	const char **symtab = utf8 ? symbols_utf8 : symbols_ascii;

	for (int i = 0; i < MAXVARS; i++) {
		if ((a.vars & UINT64_C(1)<<i) != 0)
			printf("%c ", i < 26 ? i + 'A' : i + 'a' - 26);
	}
	printf("%s ", symtab[TBLVBAR]);
	int eqnw = eqnprint(a.eqn);
	putchar('\n');

	int varcnt = popcnt(a.vars);

	for (int i = 0; i < varcnt*2; i++)
		fputs(symtab[TBLHBAR], stdout);
	fputs(symtab[TBLCROS], stdout);
	for (int i = 0; i <= eqnw; i++)
		fputs(symtab[TBLHBAR], stdout);
	putchar('\n');

	for (uint64_t msk = 0; msk < (UINT64_C(1) << varcnt); msk++) {
		for (int i = varcnt; i --> 0;)
			printf("%d ", (bool)(msk & UINT64_C(1)<<i));
		printf("%s % *d\n",
			symtab[TBLVBAR],
			(eqnw & 1) == 0 ? eqnw / 2 : eqnw/2 + 1,
			eqnsolve(a.eqn, a.vars, msk));
	}

	eqnfree(a.eqn);
}

void
astprocess_latex(ast_t a)
{
	fputs("\\begin{displaymath}\n\t\\begin{array}{|", stdout);
	int varcnt = popcnt(a.vars);
	for (int i = 0; i < varcnt; i++) {
		if (i == 0)
			putchar('c');
		else
			fputs(" c", stdout);
	}
	fputs("|c|}\n\t\t", stdout);

	for (int i = 0; i < MAXVARS; i++) {
		if ((a.vars & UINT64_C(1)<<i) != 0)
			printf("%c & ", i < 26 ? i + 'A' : i + 'a' - 26);
	}

	(void)eqnprint(a.eqn);
	puts("\\\\\n\t\t\\hline");

	for (uint64_t msk = 0; msk < (UINT64_C(1) << varcnt); msk++) {
		fputs("\t\t", stdout);
		for (int i = varcnt; i --> 0;)
			printf("%d & ", (bool)(msk & UINT64_C(1)<<i));
		printf("%d\\\\\n", eqnsolve(a.eqn, a.vars, msk));
	}

	puts("\t\\end{array}\n\\end{displaymath}");

	eqnfree(a.eqn);
}

bool
eqnsolve(eqn_t *e, uint64_t vars, uint64_t msk)
{
	switch (e->type) {
	case IDENT: {
		int i = 0, bitnr = islower(e->ch) ? e->ch-'a'+26 : e->ch-'A';
		for (int j = 0; j < bitnr; j++)
			i += (bool)(vars & UINT64_C(1)<<j);
		return msk & UINT64_C(1)<<i;
	}
	case OPAR:
		return eqnsolve(e->rhs, vars, msk);
	case NOT:
		return !eqnsolve(e->rhs, vars, msk);
	case AND:
		return eqnsolve(e->lhs, vars, msk) && eqnsolve(e->rhs, vars, msk);
	case OR:
		return eqnsolve(e->lhs, vars, msk) || eqnsolve(e->rhs, vars, msk);
	case XOR:
		return eqnsolve(e->lhs, vars, msk) ^ eqnsolve(e->rhs, vars, msk);
	case IMPL:
		return !eqnsolve(e->lhs, vars, msk) || eqnsolve(e->rhs, vars, msk);
	case EQUIV:
		return !(eqnsolve(e->lhs, vars, msk) ^ eqnsolve(e->rhs, vars, msk));
	}

#if __has_builtin(__builtin_unreachable)
	__builtin_unreachable();
#else
	abort();
#endif
}

int
eqnprint(eqn_t *a)
{
	typedef struct {
		const char *s;
		int w;
	} sym_t;

	static const sym_t symbols_utf8[] = {
		[NOT   - NOT] = {"¬", 1},
		[OR    - NOT] = {"∨", 1},
		[AND   - NOT] = {"∧", 1},
		[XOR   - NOT] = {"⊻", 1},
		[IMPL  - NOT] = {"⇒", 1},
		[EQUIV - NOT] = {"⇔", 1},
	};
	static const sym_t symbols_ascii[] = {
		[NOT   - NOT] = {"!",   1},
		[OR    - NOT] = {"||",  2},
		[AND   - NOT] = {"&&",  2},
		[XOR   - NOT] = {"^",   1},
		[IMPL  - NOT] = {"=>",  2},
		[EQUIV - NOT] = {"<=>", 3},
	};
	static const sym_t symbols_latex[] = {
		[NOT   - NOT] = {"\\lnot ",   -1},
		[OR    - NOT] = {"\\lor",     -1},
		[AND   - NOT] = {"\\land",    -1},
		[XOR   - NOT] = {"\\oplus",   -1},
		[IMPL  - NOT] = {"\\implies", -1},
		[EQUIV - NOT] = {"\\iff",     -1},
	};

	const sym_t *symtab
		= lflag ? symbols_latex
		: utf8  ? symbols_utf8
		:         symbols_ascii;

	switch (a->type) {
	case IDENT:
		putchar(a->ch);
		return 1;
	case NOT:
		fputs(symtab[NOT - NOT].s, stdout);
		return eqnprint(a->rhs) + symtab[NOT - NOT].w;
	case OPAR: {
		putchar('(');
		int w = eqnprint(a->rhs);
		putchar(')');
		return w + 2;
	}
	}

	sym_t sym = symtab[a->type - NOT];
	int w = sym.w + 2;
	w += eqnprint(a->lhs);
	printf(" %s ", sym.s);
	w += eqnprint(a->rhs);
	return w;
}

void
eqnfree(eqn_t *e)
{
	switch (e->type) {
	case IDENT:
		break;
	case NOT:
	case OPAR:
		eqnfree(e->rhs);
		break;
	default:
		eqnfree(e->lhs);
		eqnfree(e->rhs);
	}
	free(e);
}

void
user_error(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vwarnx(fmt, ap);
	va_end(ap);
	if (interactive)
		rv = EXIT_FAILURE;
	else
		exit(EXIT_FAILURE);
}
