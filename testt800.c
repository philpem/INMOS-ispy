#define PROGRAM_NAME "test_T800"

#include <stdio.h>

#include "inmos.h"
#include "checklib.h"
#include "cklib.h"
#include "test800a.h"
#include "testcode.h"

#ifndef __STDC__
int            testT800(TheLink,p,logging)
LINK TheLink;
 struct tpstats *p;
 char logging;
#else
int            testT800(LINK TheLink, struct tpstats *p, char logging)
#endif /* __STDC__ */

{
    struct testinfo *q;
    q = (struct testinfo *) p->extra;
    q->tested = TRUE;
    q->stage = "TEST800A";
    if (load(TheLink, p,
	     test800a_code.CodeSize,
	     test800a_code.Offset,
	     test800a_code.WorkSpace,
	     test800a_code.VectorSpace,
	     test800a_code.BytesPerWord,
	     test800a_code.Code))
    {
    	setroute(TheLink, p->parent, p->route);
	if (!getresult(TheLink, p, logging))
	    return (FALSE);		/* give up on this one */
    } else
	return load_failed(p);
    return(TRUE);
}
