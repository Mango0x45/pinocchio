#include <ctype.h>
#include <err.h>
#include <langinfo.h>
#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"

#define MAXVARS (26 * 2)

#ifndef __has_builtin
#	define __has_builtin(x) (0)
#endif

static bool utf8;

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
	setlocale(LC_ALL, "");
	utf8 = strcmp(nl_langinfo(CODESET), "UTF-8") == 0;
	if (argc > 1) {
		if ((yyin = fopen(argv[1], "r")) == NULL)
			err(1, "fopen: %s", argv[1]);
	}
	return yyparse();
}

void
astprocess(ast_t a)
{
	if (a.eqn == NULL)
		return;

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
		bool set = a.vars & UINT64_C(1)<<i;
		if (set)
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
		for (int i = varcnt; i --> 0;) {
			bool bit = msk & 1<<i;
			printf("%d ", bit);
		}
		printf("%s ", symtab[TBLVBAR]);
		int w = (eqnw & 1) == 0 ? eqnw / 2 : eqnw/2 + 1;
		printf("% *d\n", w, eqnsolve(a.eqn, a.vars, msk));
	}

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

	const sym_t *symtab = utf8 ? symbols_utf8 : symbols_ascii;

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
