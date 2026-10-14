#define PROGRAM_NAME "test_T32"

#include <stdio.h>

#include "inmos.h"
#include "checklib.h"
#include "cklib.h"
#include "testt32a.h"
#include "testt32b.h"
#include "testcode.h"

#ifndef __STDC__
int            testT32(TheLink,p,logging)
LINK TheLink;
 struct tpstats *p;
 char logging;
#else
int            testT32(LINK TheLink, struct tpstats *p, char logging)
#endif /* __STDC__ */
{
    struct testinfo *q;
    q = (struct testinfo *) p->extra;
    q->tested = TRUE;
    q->stage = "TESTT32A";
    if (load(TheLink, p,
	     testt32a_code.CodeSize,
	     testt32a_code.Offset,
	     testt32a_code.WorkSpace,
	     testt32a_code.VectorSpace,
	     testt32a_code.BytesPerWord,
	     testt32a_code.Code))
    {
    	setroute(TheLink, p->parent, p->route);
    	if ((p->tptype == T414B) ||	/* a breakpointing TP ? */
	    (p->tptype == T800C) ||
	    (p->tptype == T800D)   )
	    sendiserver(TheLink, 1, (unsigned char *) "\0");
	else
	    sendiserver(TheLink, 1, (unsigned char *) "\1");
	if (!getresult(TheLink, p, logging))
	    return (0);		/* give up on this one */
    } else
	AbortExit(PROGRAM_NAME, "Failed to load Transputer %d - TESTT32A", p->tpid);
    q->stage = "TESTT32B";
    if (load(TheLink, p,
	     testt32b_code.CodeSize,
	     testt32b_code.Offset,
	     testt32b_code.WorkSpace,
	     testt32b_code.VectorSpace,
	     testt32b_code.BytesPerWord,
	     testt32b_code.Code))
    {
    	setroute(TheLink, p->parent, p->route);
	if (!getresult(TheLink, p, logging))
	    return (0);		/* give up on this one */
    } else
	AbortExit(PROGRAM_NAME, "Failed to load Transputer %d - TESTT32B", p->tpid);
    return (1);
}
