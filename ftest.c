
/* --------------------------------------------------
 *
 *    ftest.c  --  functional test program
 *
 *    Andy Rabagliati INMOS
 *
 * --------------------------------------------------
 */

char           *PROGRAM_NAME = "ftest";
#define VERSION_NUMBER  "3.0"
#define VERSION_DATE    "25 Jan 1991"

/*{{{  includes*/
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#ifdef MSC
#include <process.h>
#endif

#include "inmos.h"
#include "checklib.h"
#include "cklib.h"
#include "testcode.h"
#include "test4k.h"
#include "route16.h"
#include "route32.h"

/*}}}  */
/*{{{  defines*/

#define TIMEOUT 5

#define STDERR  stderr

#define HUNKY_DORY 0
#define BAD_KARMA 1

static int AnalyseFailure = FALSE;
static int SelectedProcessor = -1;
static int SkipMicrocode = FALSE;
static int ProbeLater = FALSE;
static int ModernPart = 0;

#ifdef MSC
#define SWITCHAR '/'
#else
#define SWITCHAR '-'
#endif
/*}}}  */

/*{{{  Usage*/
#ifndef __STDC__
void            Usage()
#else
void            Usage(void)
#endif				/* __STDC__ */
{
                    fprintf(stdout, "\n%s  --  version %s of %s.\n", PROGRAM_NAME,
			                    VERSION_NUMBER, VERSION_DATE);
    fprintf(stdout, "\nUsage:  ispy | %s [%coption...]\n",
	    PROGRAM_NAME, SWITCHAR);
    fprintf(stdout, "Valid options are: \n");
    fprintf(stdout, "     %cT2        :  Test M212s, T2s, T225s only\n", SWITCHAR);
    fprintf(stdout, "     %cT4        :  Test T414s, T425s only\n", SWITCHAR);
    fprintf(stdout, "     %cT8        :  Test T800s, T801s, T805s only\n", SWITCHAR);
    fprintf(stdout, "     %cL         :  Log progress of testing\n", SWITCHAR);
    fprintf(stdout, "     %cA         :  Analyse a stopped 32-bit root after a transport failure\n", SWITCHAR);
    fprintf(stdout, "     %cN number  :  Test only this processor; retain other routers\n", SWITCHAR);
    fprintf(stdout, "     %cS         :  Skip chip microcode tests (reduced coverage)\n", SWITCHAR);
    fprintf(stdout, "     %cP         :  Run only the later-revision priority/timer probe\n", SWITCHAR);
    fprintf(stdout, "     %cD name    :  Select later microcode subtest: cm, stack, const, inc\n", SWITCHAR);
    exit(0);
}

/*}}}  */
/*{{{  getresult*/
#ifndef __STDC__
int             getresult(TheLink, p, logging)
    LINK            TheLink;
    struct tpstats *p;
    char            logging;
#else
int             getresult(LINK TheLink, struct tpstats * p, char logging)
#endif				/* __STDC__ */
{
    unsigned char packet[128];
    unsigned int length;
    struct testinfo *q;
    q = (struct testinfo *) p->extra;
    if (logging)
        fprintf(stderr, "\n# Processor %d: %s\n", p->tpid,
                q->stage != NULL ? q->stage : "unknown test");
    for (;;) {
        size_t used, available, count;
        const char *failure = NULL;
        length = 0;
        if (!getiserver(TheLink, &length, packet, sizeof(packet) - 1)) {
            int error = TestError(TheLink);
            fprintf(stderr, "# Link Error indicator: %s\n",
                    error < 0 ? "unavailable" : error ? "asserted" : "clear");
            failure = "TIMEOUT OR READ ERROR";
        } else if (length == 0) {
            failure = "EMPTY RESULT";
        } else {
            packet[length] = '\0';
            if (packet[0] == '.')
                return TRUE;
            if (packet[0] == ' ') {
                if (logging)
                    fprintf(stderr, "# Processor %d: PASS %s\n", p->tpid, packet + 1);
                continue;
            }
            if (packet[0] == ':') {
                if (logging)
                    fprintf(stderr, "# Processor %d: %s\n", p->tpid, packet + 1);
                continue;
            }
            if (packet[0] != '?')
                failure = "BAD RESULT";
        }
        q->error = TRUE;
        used = strlen(q->errors);
        available = sizeof(q->errors) - used - 1;
        if (failure != NULL) {
            fprintf(stderr, "# Processor %d: %s: %s\n", p->tpid,
                    q->stage != NULL ? q->stage : "unknown test", failure);
            count = strlen(failure);
        } else {
            if (logging)
                fprintf(stderr, "# Processor %d: FAIL %s\n", p->tpid, packet + 1);
            count = length - 1;
        }
        if (available > 0) {
            q->errors[used++] = ',';
            available--;
        }
        if (count > available) {
            memcpy(q->errors + used, failure != NULL ? failure : (char *) packet + 1,
                   available);
            q->errors[used + available] = '\0';
            memcpy(q->errors + sizeof(q->errors) - 4, "...", 4);
        } else {
            memcpy(q->errors + used, failure != NULL ? failure : (char *) packet + 1,
                   count);
            q->errors[used + count] = '\0';
        }
        if (failure != NULL)
            return FALSE;
    }
}
/*}}}  */

/* Capture the root with the historical CKMON state bootstrap. Analyse
 * preserves stopped I and W in A and B on T805; it changes network state.
 */
static unsigned long stateword(const unsigned char *bytes)
{
    unsigned long word = 0;
    int i;
    for (i = 3; i >= 0; i--)
        word = (word << 8) | bytes[i];
    return word;
}

static void analyse_root(LINK link, struct tpstats *p)
{
#include "peek.h"
    unsigned char state[32];
    unsigned int length = 0;
    if (p->parent != NULL || bpw(p->tptype) != 4) {
        fprintf(stderr, "# Analyse snapshot supports a 32-bit root only\n");
        return;
    }
    fprintf(stderr, "# Analysing stopped root; rediscover network afterwards\n");
    if (AnalyseLink(link) < 0 ||
        WriteLink(link, getstate, sizeof(getstate), 40) != sizeof(getstate) ||
        !getiserver(link, &length, state, sizeof(state)) || length != sizeof(state)) {
        fprintf(stderr, "# Root Analyse snapshot unavailable\n");
        return;
    }
    fprintf(stderr, "# Root snapshot: Iptr=%08lX Wdesc=%08lX boot=%08lX flags=%08lX\n",
            stateword(state + 4), stateword(state + 8), stateword(state + 12),
            stateword(state));
    fprintf(stderr, "# Root queues: high=%08lX,%08lX low=%08lX,%08lX\n",
            stateword(state + 16), stateword(state + 20),
            stateword(state + 24), stateword(state + 28));
}

/*{{{  sortl*/
#ifndef __STDC__
struct tpstats *sortl(root)
    struct tpstats *root;	/* by route length */
#else
struct tpstats *sortl(struct tpstats * root)	/* by route length */
#endif				/* __STDC__ */
{
    struct tpstats **p;
    int             sorted;
    sorted = FALSE;
    while (!sorted)
    {
	sorted = TRUE;
	for (p = (&root); ((*p) != NULL) && ((*p)->next != NULL); p = (&((*p)->next)))
	    if ((*p)->routelen > (*p)->next->routelen)
	    {
		struct tpstats *temp;
		temp = *p;
		*p = temp->next;
		temp->next = temp->next->next;
		(*p)->next = temp;
		sorted = FALSE;
	    }
    }
    return (root);
}
/*}}}  */
/*{{{  sortid*/
#ifndef __STDC__
struct tpstats *sortid(root)
    struct tpstats *root;
#else
struct tpstats *sortid(struct tpstats * root)
#endif				/* __STDC__ */
{
    struct tpstats **p;
    int             sorted;
    sorted = FALSE;
    while (!sorted)
    {
	sorted = TRUE;
	for (p = (&root); ((*p) != NULL) && ((*p)->next != NULL); p = (&((*p)->next)))
	    if ((*p)->tpid > (*p)->next->tpid)
	    {
		struct tpstats *temp;
		temp = *p;
		*p = temp->next;
		temp->next = temp->next->next;
		(*p)->next = temp;
		sorted = FALSE;
	    }
    }
    return (root);
}
/*}}}  */

/*{{{  getparams(argc, argv, testT2s, testT4s, testT8s, logging)*/
#ifndef __STDC__
void            getparams(argc, argv, testT2s, testT4s, testT8s, logging)
    int             argc;
    char           *argv[];

char           *testT2s;
char           *testT4s;
char           *testT8s;

char           *logging;
#else
void            getparams(int argc, char *argv[],
		                char *testT2s, char *testT4s, char *testT8s,
			                  char *logging)
#endif				/* __STDC__ */
{
                  ++argv;
    while (--argc > 0)
    {
	char           *c;
	if ((char) (**argv) != SWITCHAR)
	    Usage();
	c = *argv;
	++c;
	switch (*c)
	{
	    case '?':
	    case 'h':
	    case 'H':
		Usage();
		break;
	    case 'l':
	    case 'L':
		*logging = TRUE;
		break;
	    case 'a':
	    case 'A':
		AnalyseFailure = TRUE;
		break;
            case 's':
            case 'S':
                SkipMicrocode = TRUE;
                break;
            case 'p':
            case 'P':
                ProbeLater = TRUE;
                break;
	    case 'T':
	    case 't':
		++c;
		switch (*c)
		{
		    case '2':
			*testT2s = TRUE;
			break;
		    case '4':
			*testT4s = TRUE;
			break;
		    case '8':
			*testT8s = TRUE;
			break;
		}
		break;
	    case 'n':
	    case 'N':
                {
                    char *end;
                    long number;
                    if (argc < 2 || argv[1][0] == '\0')
                        AbortExit(PROGRAM_NAME, "Missing processor number for -n");
                    number = strtol(argv[1], &end, 10);
                    if (end == argv[1] || *end != '\0' || number < 0 || number > 32767)
                        AbortExit(PROGRAM_NAME, "Invalid processor number for -n");
                    SelectedProcessor = (int) number;
                    ++argv;
                    --argc;
                }
                break;
            case 'd':
            case 'D':
                if (argc < 2)
                    AbortExit(PROGRAM_NAME, "Missing subtest name for -d");
                ++argv;
                --argc;
                if (!strcmp(*argv, "cm")) ModernPart = 1;
                else if (!strcmp(*argv, "stack")) ModernPart = 2;
                else if (!strcmp(*argv, "const")) ModernPart = 3;
                else if (!strcmp(*argv, "inc")) ModernPart = 4;
                else AbortExit(PROGRAM_NAME, "Unknown subtest name for -d");
                break;
	    default:
		Usage();
		break;
	}
	++argv;
    }

    if ((*testT2s == FALSE) && (*testT4s == FALSE) && (*testT8s == FALSE))
	*testT2s = *testT4s = *testT8s = TRUE;
    if ((ProbeLater && (SkipMicrocode || ModernPart)) || (SkipMicrocode && ModernPart))
        AbortExit(PROGRAM_NAME, "Use only one of -p, -s or -d");
}
/*}}}  */
static void skip_chip(struct tpstats *p, const char *name)
{
    struct testinfo *q = (struct testinfo *) p->extra;
    q->skipped = name;
    fprintf(stderr, "# Processor %d: SKIP %s (reduced coverage)\n", p->tpid, name);
}

int load_failed(struct tpstats *p)
{
    struct testinfo *q = (struct testinfo *) p->extra;
    q->error = TRUE;
    q->tested = TRUE;
    strncat(q->errors, ",LOAD ERROR", sizeof(q->errors) - strlen(q->errors) - 1);
    fprintf(stderr, "# Processor %d: %s: LOAD ERROR\n", p->tpid, q->stage);
    return FALSE;
}

static int restore_router(LINK link, struct tpstats *p)
{
    struct testinfo *q = (struct testinfo *) p->extra;
    int loaded;
    if (bpw(p->tptype) == 2) {
        q->stage = "ROUTE16";
        loaded = load(link, p, route16_code.CodeSize, route16_code.Offset,
                      route16_code.WorkSpace, route16_code.VectorSpace,
                      route16_code.BytesPerWord, route16_code.Code);
    } else {
        q->stage = "ROUTE32";
        loaded = load(link, p, route32_code.CodeSize, route32_code.Offset,
                      route32_code.WorkSpace, route32_code.VectorSpace,
                      route32_code.BytesPerWord, route32_code.Code);
    }
    if (!loaded)
        return load_failed(p);
    return TRUE;
}

/*{{{  main(argc, argv)*/
#ifndef __STDC__
int             main(argc, argv)
    int             argc;
    char           *argv[];
#else
int             main(int argc, char *argv[])
#endif				/* __STDC__ */
{
    /* {{{  DECLs */
    struct tpstats *root, *p;
    struct testinfo *q;
    LINK            TheLink;
    char            Pipe[128], Banner[128];
    char            LinkName[64];
    int             transputers, ok, failures = FALSE;
    char            testT2s = FALSE, testT4s = FALSE, testT8s = FALSE, logging = FALSE;
    /* }}}  */
    getparams(argc, argv, &testT2s, &testT4s, &testT8s, &logging);
    transputers = lex(stdin, &root, Pipe, LinkName, Banner);
    if (transputers == 0)
	AbortExit(PROGRAM_NAME, "No transputers found");
    if (SelectedProcessor >= 0) {
        for (p = root; p != NULL && p->tpid != SelectedProcessor; p = p->next)
            ;
        if (p == NULL)
            AbortExit(PROGRAM_NAME, "Processor %d not found", SelectedProcessor);
    }
    TheLink = OpenLink(LinkName);
    ok = (TheLink > 0);
    if (!ok)
	AbortExit(PROGRAM_NAME, "Error opening link name \"%s\"", LinkName);
    root = sortl(root);
    for (p = root; p != NULL; p = p->next)
    {
	p->extra = Malloc(sizeof(struct testinfo));
	q = (struct testinfo *) p->extra;
	q->error = FALSE;
	q->tested = FALSE;
	q->errors[0] = '\0';
	q->stage = NULL;
	q->skipped = NULL;
    }
    for (p = root; p != NULL; p = p->next)
    {
	q = (struct testinfo *) p->extra;
	if (SelectedProcessor >= 0 && p->tpid != SelectedProcessor)
	    continue;
        switch (class(p->tptype)) {
            case T_212: case M212: case T_225:
                if (!testT2s) continue;
                break;
            case T_414: case T_425:
                if (!testT4s) continue;
                break;
            case T_800: case T_801: case T_805:
                if (!testT8s) continue;
                break;
            default:
                continue;
        }
        if (ProbeLater) {
            if (!later_supported(p->tptype)) {
                skip_chip(p, "priority/timer probe: unsupported revision");
                continue;
            }
            stop(p);
            if (!testLater(TheLink, p, logging, -1))
                goto transport_failed;
            q->skipped = "full diagnostic suite (probe-only run)";
            if (!restore_router(TheLink, p))
                goto transport_failed;
            continue;
        }
        if (ModernPart && !later_supported(p->tptype)) {
            skip_chip(p, "selected microcode test: unsupported revision");
            continue;
        }
	switch (class(p->tptype))
	{
	    case T_212:
	    case M212:
	    case T_225:
		if (testT2s)
		{
		    stop(p);
		    if (!testT2(TheLink, p, logging))
			goto transport_failed;
                    if (!restore_router(TheLink, p))
                        goto transport_failed;
		}
		break;
	    case T_414:
		if (testT4s)
		{
		    stop(p);
		    if (!testT32(TheLink, p, logging))
			goto transport_failed;
                    if (SkipMicrocode)
                        skip_chip(p, "TEST414A");
                    else if (!testT414(TheLink, p, logging))
                        goto transport_failed;
                    if (!restore_router(TheLink, p))
                        goto transport_failed;
		}
		break;
	    case T_800:
		if (testT8s)
		{
		    stop(p);
		    if (!testT32(TheLink, p, logging))
			goto transport_failed;
		    if (!testT80x(TheLink, p, logging))
			goto transport_failed;
                    if (SkipMicrocode)
                        skip_chip(p, "TEST800A");
                    else if (p->tptype == T800C || p->tptype == T800D) {
                        if (!testT800(TheLink, p, logging))
                            goto transport_failed;
                    } else
                        skip_chip(p, "T800 microcode tests: unsupported revision");
                    if (!restore_router(TheLink, p))
                        goto transport_failed;
		}
		break;
	    case T_425:
		if (testT4s)
		{
		    stop(p);
		    if (!testT32(TheLink, p, logging))
			goto transport_failed;
                    if (SkipMicrocode)
                        skip_chip(p, "T425 microcode tests");
                    else if (later_supported(p->tptype)) {
                        if (!testLater(TheLink, p, logging, ModernPart))
                            goto transport_failed;
                        if (ModernPart) q->skipped = "other three microcode subtests";
                    } else if (p->tptype == T_425) {
                        if (!testT425(TheLink, p, logging))
                            goto transport_failed;
                    } else
                        skip_chip(p, "T425 microcode tests: unsupported revision");
                    if (!restore_router(TheLink, p))
                        goto transport_failed;
		}
		break;
	    case T_801:
	    case T_805:
		if (testT8s)
		{
		    stop(p);
		    if (!testT32(TheLink, p, logging))
			goto transport_failed;
		    if (!testT80x(TheLink, p, logging))
			goto transport_failed;
                    if (SkipMicrocode) {
                        skip_chip(p, "T801/T805 microcode tests");
                    } else if (later_supported(p->tptype)) {
                        if (!testLater(TheLink, p, logging, ModernPart))
                            goto transport_failed;
                        if (ModernPart) q->skipped = "other three microcode subtests";
                    } else
                        skip_chip(p, "T801/T805 microcode tests: unsupported revision");
                    if (!restore_router(TheLink, p))
                        goto transport_failed;
		}
		break;
	}
	if (logging && q->error)
	    fprintf(stderr, "\015TP %5d:          FAILURES\n", p->tpid);
    }
    goto report_results;
transport_failed:
    ok = FALSE;
    if (AnalyseFailure)
        analyse_root(TheLink, p);
    fprintf(stderr, "# Stopping after processor %d; run ISpy again before retrying.\n",
            p->tpid);
report_results:
    root = sortid(root);
    /* {{{  print results */
    if (logging)
	fprintf(stderr, "\015                             \015");
    printf("%s | %s %s\n%s (Ftest)\n", Pipe, PROGRAM_NAME, VERSION_NUMBER,
	   Banner);
    for (p = root; p != NULL; p = p->next)
    {
	q = (struct testinfo *) p->extra;
	if (q->error)
	    failures = TRUE;
	if (!q->tested)
	    printf("%s (NOT TESTED)\n", p->info);
	else
	{
	    if (q->error)
	    {
		printf("%s (BAD)\n", p->info);
		printf("# Errors%s\n", q->errors);
            } else if (q->skipped != NULL)
                printf("%s (PARTIAL)\n", p->info);
            else
		printf("%s  (OK)\n", p->info);
	}
        if (q->skipped != NULL) {
            printf("# Skipped %s\n", q->skipped);
            if (!ProbeLater && !ModernPart)
                printf("# Omitted Conditional Mux, Stack drivers, Constants, Incrementer\n");
        }
    }
    /* }}}  */
    CloseLink(TheLink);
    return (ok && !failures ? HUNKY_DORY : BAD_KARMA);
}
/*}}}  */


/*
 *  eof
 */
