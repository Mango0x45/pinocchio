#include <assert.h>
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
#include <unistd.h>

#include "lexer.h"
#include "parser.h"
#include "wrapper.h"

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MAXVARS   (26 * 2)

#define lengthof(x) ((sizeof(x) / sizeof(*(x))))

#ifndef __has_builtin
#	define __has_builtin(x) (0)
#endif

typedef enum {
	TS_UNSET = 0,
	TS_ASCII,
	TS_LATEX,
	TS_UTF8,
} tbl_style_t;

typedef enum {
	BS_ALPHA,
	BS_BINARY,
	BS_SYMBOLS,
} bool_style_t;

static int rv;
static bool interactive;
static bool_style_t bflag = BS_BINARY;
static tbl_style_t tflag = TS_UNSET;

const char *current_file;

static void astprocess_cli(asts_t);
static void astprocess_latex(asts_t);
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
	interactive = isatty(STDIN_FILENO);

	int opt;
	static struct option longopts[] = {
		{"bool-style",  required_argument, 0, 'b'},
		{"table-style", required_argument, 0, 't'},
		{0},
	};

	while ((opt = getopt_long(argc, argv, "b:t:", longopts, NULL)) != -1) {
		switch (opt) {
		case 'b':
			if (streq(optarg, "alpha"))
				bflag = BS_ALPHA;
			else if (streq(optarg, "binary"))
				bflag = BS_BINARY;
			else if (streq(optarg, "symbols"))
				bflag = BS_SYMBOLS;
			else {
				warnx("invalid bool style -- '%s'", optarg);
				goto usage;
			}
			break;
		case 't':
			if (streq(optarg, "ascii"))
				tflag = TS_ASCII;
			else if (streq(optarg, "latex"))
				tflag = TS_LATEX;
			else if (streq(optarg, "utf8"))
				tflag = TS_UTF8;
			else {
				warnx("invalid table style -- '%s'", optarg);
				goto usage;
			}
			break;
		default:
usage:
			fprintf(stderr, "Usage: %s [-b alpha|binary|symbols] [-t ascii|latex|utf8] [file ...]\n",
				argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	if (tflag == TS_UNSET) {
		tflag = streq(nl_langinfo(CODESET), "UTF-8")
			? TS_UTF8 : TS_ASCII;
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
		if (streq(argv[i], "-"))
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
astprocess(asts_t as)
{
	(tflag == TS_LATEX ? astprocess_latex : astprocess_cli)(as);
	for (size_t i = 0; i < as.len; i++)
		eqnfree(as.buf[i].eqn);
	free(as.buf);
}

void
astprocess_cli(asts_t as)
{
	enum {
		TBLVBAR,
		TBLHBAR,
		TBLCROS,
	};

	static const char *tblsyms_utf8[] = {
		[TBLVBAR] = "│",
		[TBLHBAR] = "─",
		[TBLCROS] = "┼",
	};
	static const char *tblsyms_ascii[] = {
		[TBLVBAR] = "|",
		[TBLHBAR] = "-",
		[TBLCROS] = "+",
	};

	static const char *boolsyms[][2] = {
		[BS_ALPHA]   = {"F", "T"},
		[BS_BINARY]  = {"0", "1"},
		[BS_SYMBOLS] = {"⊥", "⊤"},
	};

	const char **tblsyms = tflag == TS_UTF8 ? tblsyms_utf8 : tblsyms_ascii;

	uint64_t varmsk = as.buf[0].vars;
	for (size_t i = 1; i < as.len; i++)
		varmsk |= as.buf[i].vars;
	int varcnt = popcnt(varmsk);

	for (int i = 0; i < MAXVARS; i++) {
		if ((varmsk & UINT64_C(1)<<i) != 0)
			printf("%c ", i < 26 ? i + 'A' : i + 'a' - 26);
	}

	int *eqnws = xmalloc(sizeof(*eqnws) * as.len);

	for (size_t i = 0; i < as.len; i++) {
		printf("%s ", tblsyms[TBLVBAR]);
		eqnws[i] = eqnprint(as.buf[i].eqn);
		if (i < as.len - 1) {
			eqnws[i]++;
			putchar(' ');
		}
	}
	putchar('\n');

	for (int i = 0; i < varcnt*2; i++)
		fputs(tblsyms[TBLHBAR], stdout);
	fputs(tblsyms[TBLCROS], stdout);
	for (size_t i = 0; i < as.len; i++) {
		for (int j = 0; j <= eqnws[i]; j++)
			fputs(tblsyms[TBLHBAR], stdout);
		if (i < as.len - 1)
			fputs(tblsyms[TBLCROS], stdout);
	}

	putchar('\n');

	for (uint64_t msk = 0; msk < (UINT64_C(1) << varcnt); msk++) {
		for (int i = varcnt; i --> 0;)
			printf("%s ", boolsyms[bflag][(bool)(msk & UINT64_C(1)<<i)]);

		for (size_t i = 0; i < as.len; i++) {
			int eqnw = eqnws[i];
			int w = (eqnw & 1) == 0 ? eqnw / 2 : eqnw/2 + 1;
			printf("%s %*s",
				tblsyms[TBLVBAR],
				/* The symbols are encoded as 3 UTF-8 codepoints */
				w + (bflag == BS_SYMBOLS)*2,
				boolsyms[bflag][eqnsolve(as.buf[i].eqn, varmsk, msk)]);
			if (i < as.len - 1) {
				if ((eqnw & 1) != 0)
					w--;
				for (int i = 0; i < w; i++)
					putchar(' ');
			}
		}

		putchar('\n');
	}

	free(eqnws);
}

void
astprocess_latex(asts_t as)
{
	static const char *boolsyms[][2] = {
		[BS_ALPHA]   = {"F", "T"},
		[BS_BINARY]  = {"0", "1"},
		[BS_SYMBOLS] = {"\\bot", "\\top"},
	};

	fputs("\\begin{displaymath}\n\t\\begin{array}{|", stdout);

	uint64_t varmsk = as.buf[0].vars;
	for (size_t i = 1; i < as.len; i++)
		varmsk |= as.buf[i].vars;
	int varcnt = popcnt(varmsk);

	for (int i = 0; i < varcnt; i++) {
		if (i > 0)
			putchar(' ');
		putchar('c');
	}

	for (size_t i = 0; i < as.len; i++)
		fputs("|c", stdout);
	fputs("|}\n\t\t", stdout);

	for (int i = 0; i < MAXVARS; i++) {
		if ((varmsk & UINT64_C(1)<<i) != 0)
			printf("%c & ", i < 26 ? i + 'A' : i + 'a' - 26);
	}

	for (size_t i = 0; i < as.len; i++) {
		(void)eqnprint(as.buf[i].eqn);
		if (i < as.len - 1)
			fputs(" & ", stdout);
	}
	puts("\\\\\n\t\t\\hline");

	for (uint64_t msk = 0; msk < (UINT64_C(1) << varcnt); msk++) {
		fputs("\t\t", stdout);
		for (int i = varcnt; i --> 0;)
			printf("%s & ", boolsyms[bflag][(bool)(msk & UINT64_C(1)<<i)]);

		for (size_t i = 0; i < as.len; i++) {
			printf("%s", boolsyms[bflag][eqnsolve(as.buf[i].eqn, varmsk, msk)]);
			if (i < as.len - 1)
				fputs(" & ", stdout);
		}
		fputs("\\\\\n", stdout);
	}

	puts("\t\\end{array}\n\\end{displaymath}");
}

bool
eqnsolve(eqn_t *e, uint64_t vars, uint64_t msk)
{
	switch (e->type) {
	case IDENT: {
		int i = 0, bitnr = islower(e->ch) ? e->ch-'a'+26 : e->ch-'A';
		for (int j = 0; j < bitnr; j++)
			i += (bool)(vars & UINT64_C(1)<<j);

		/* We have a bit of a problem, where if given the input
		   ‘a && b | b && c’, when extracting the value of ‘c’ we
		   actually extract the value of ‘a’ because MSK is backwards.

		   This means that if we have 3 variables (a, b, and c) and we
		   want to get the value of ‘c’, we need to extract bit 0 while
		   for ‘a’ we need bit 2.  This tomfoolery here does the mapping
		   of the ‘logical’ index (i.e. {0, 1, 2} for {a, b, c}) to the
		   ‘real’ index (i.e. {2, 1, 0} for {a, b, c}). */
		i = -i + popcnt(vars) - 1;

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
		[XOR   - NOT] = {"⊕", 1},
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
		= tflag == TS_LATEX ? symbols_latex
		: tflag == TS_UTF8  ? symbols_utf8
		:                     symbols_ascii
		;

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
