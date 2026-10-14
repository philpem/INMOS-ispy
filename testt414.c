#define PROGRAM_NAME "test_T414"

#include <stdio.h>

#include "inmos.h"
#include "checklib.h"
#include "cklib.h"
#include "test414a.h"
#include "testcode.h"

#ifndef __STDC__
int             testT414(TheLink,p,logging)
LINK TheLink;
 struct tpstats * p;
 char logging;
#else
int             testT414(LINK TheLink, struct tpstats * p, char logging)
#endif /* __STDC__ */

{
    struct testinfo *q;
    q = (struct testinfo *) p->extra;
    q->tested = TRUE;
    q->stage = "TEST414A";
    if (load(TheLink, p,
	     test414a_code.CodeSize,
	     test414a_code.Offset,
	     test414a_code.WorkSpace,
	     test414a_code.VectorSpace,
	     test414a_code.BytesPerWord,
	     test414a_code.Code))
    {
    	setroute(TheLink, p->parent, p->route);
	if (!getresult(TheLink, p, logging))
	    return (FALSE);	/* give up on this one */
    } else
	return load_failed(p);
    return (TRUE);
}
