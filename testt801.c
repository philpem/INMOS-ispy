#define PROGRAM_NAME "test_T801"

#include <stdio.h>

#include "inmos.h"
#include "checklib.h"
#include "cklib.h"
#include "test801a.h"
#include "testcode.h"

#ifndef __STDC__
int            testT801(TheLink,p,logging)
LINK TheLink;
 struct tpstats *p;
 char logging;
#else
int            testT801(LINK TheLink, struct tpstats *p, char logging)
#endif /* __STDC__ */

{
    struct testinfo *q;
    q = (struct testinfo *) p->extra;
    q->tested = TRUE;
    q->stage = "TEST801A";
    if (load(TheLink, p,
	     test801a_code.CodeSize,
	     test801a_code.Offset,
	     test801a_code.WorkSpace,
	     test801a_code.VectorSpace,
	     test801a_code.BytesPerWord,
	     test801a_code.Code))
    {
    	setroute(TheLink, p->parent, p->route);
	if (!getresult(TheLink, p, logging))
	    return (FALSE);		/* give up on this one */
    } else
	AbortExit(PROGRAM_NAME, "Failed to load Transputer %d - TEST801A", p->tpid);
    return(TRUE);
}
