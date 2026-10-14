#define PROGRAM_NAME "test_T425"

#include <stdio.h>

#include "inmos.h"
#include "checklib.h"
#include "cklib.h"
#include "test425a.h"
#include "testcode.h"

#ifndef __STDC__
int             testT425(TheLink,p,logging)
LINK TheLink;
 struct tpstats * p;
 char logging;
#else
int             testT425(LINK TheLink, struct tpstats * p, char logging)
#endif /* __STDC__ */

{
    struct testinfo *q;
    q = (struct testinfo *) p->extra;
    q->tested = TRUE;
    q->stage = "TEST425A";
    if (load(TheLink, p,
	     test425a_code.CodeSize,
	     test425a_code.Offset,
	     test425a_code.WorkSpace,
	     test425a_code.VectorSpace,
	     test425a_code.BytesPerWord,
	     test425a_code.Code))
    {
    	setroute(TheLink, p->parent, p->route);
	if (!getresult(TheLink, p, logging))
	    return (0);		/* give up on this one */
    } else
	AbortExit(PROGRAM_NAME, "Failed to load Transputer %d - TEST425A", p->tpid);
    return (1);
}
