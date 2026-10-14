#define PROGRAM_NAME "test_T2"

#include <stdio.h>

#include "inmos.h"
#include "checklib.h"
#include "cklib.h"
#include "test222a.h"
#include "test222b.h"
#include "testcode.h"

#ifndef __STDC__
int             testT2(TheLink,p,logging)
LINK TheLink;
 struct tpstats * p;
 char logging;
#else
int             testT2(LINK TheLink, struct tpstats * p, char logging)
#endif /* __STDC__ */
{
    struct testinfo *q;
    q = (struct testinfo *) p->extra;
    q->tested = TRUE;
    q->stage = "TEST222A";
    if (load(TheLink, p,
	     test222a_code.CodeSize,
	     test222a_code.Offset,
	     test222a_code.WorkSpace,
	     test222a_code.VectorSpace,
	     test222a_code.BytesPerWord,
	     test222a_code.Code))
    {
    	setroute(TheLink, p->parent, p->route);
	if (!getresult(TheLink, p, logging))
	    return (0);		/* give up on this one */
    } else
	AbortExit(PROGRAM_NAME, "Failed to load Transputer %d - TEST222A", p->tpid);
    q->stage = "TEST222B";
    if (load(TheLink, p,
	     test222b_code.CodeSize,
	     test222b_code.Offset,
	     test222b_code.WorkSpace,
	     test222b_code.VectorSpace,
	     test222b_code.BytesPerWord,
	     test222b_code.Code))
    {
    	setroute(TheLink, p->parent, p->route);
	if (!getresult(TheLink, p, logging))
	    return (0);		/* give up on this one */
    } else
	AbortExit(PROGRAM_NAME, "Failed to load Transputer %d - TEST222B", p->tpid);
    return (1);
}
