#define PROGRAM_NAME "test_T80X"

#include <stdio.h>

#include "inmos.h"
#include "checklib.h"
#include "cklib.h"
#include "test80xa.h"
#include "test80xb.h"
#include "test80xc.h"
#include "test80xd.h"
#include "testcode.h"

#ifndef __STDC__
int            testT80x(TheLink,p,logging)
LINK TheLink;
 struct tpstats *p;
 char logging;
#else
int            testT80x(LINK TheLink, struct tpstats *p, char logging)
#endif /* __STDC__ */

{
    struct testinfo *q;
    q = (struct testinfo *) p->extra;
    q->tested = TRUE;
    q->stage = "TEST80XA";
    if (load(TheLink, p,
	     test80xa_code.CodeSize,
	     test80xa_code.Offset,
	     test80xa_code.WorkSpace,
	     test80xa_code.VectorSpace,
	     test80xa_code.BytesPerWord,
	     test80xa_code.Code))
    {
    	setroute(TheLink, p->parent, p->route);
	if (!getresult(TheLink, p, logging))
	    return (FALSE);		/* give up on this one */
    } else
	AbortExit(PROGRAM_NAME, "Failed to load Transputer %d - TEST80XA", p->tpid);
    q->stage = "TEST80XB";
    if (load(TheLink, p,
	     test80xb_code.CodeSize,
	     test80xb_code.Offset,
	     test80xb_code.WorkSpace,
	     test80xb_code.VectorSpace,
	     test80xb_code.BytesPerWord,
	     test80xb_code.Code))
    {
    	setroute(TheLink, p->parent, p->route);
	if (!getresult(TheLink, p, logging))
	    return (FALSE);		/* give up on this one */
    } else
	AbortExit(PROGRAM_NAME, "Failed to load Transputer %d - TEST80XB", p->tpid);
    q->stage = "TEST80XC";
    if (load(TheLink, p,
	     test80xc_code.CodeSize,
	     test80xc_code.Offset,
	     test80xc_code.WorkSpace,
	     test80xc_code.VectorSpace,
	     test80xc_code.BytesPerWord,
	     test80xc_code.Code))
    {
    	setroute(TheLink, p->parent, p->route);
	if (!getresult(TheLink, p, logging))
	    return (FALSE);		/* give up on this one */
    } else
	AbortExit(PROGRAM_NAME, "Failed to load Transputer %d - TEST80XC", p->tpid);
    q->stage = "TEST80XD";
    if (load(TheLink, p,
	     test80xd_code.CodeSize,
	     test80xd_code.Offset,
	     test80xd_code.WorkSpace,
	     test80xd_code.VectorSpace,
	     test80xd_code.BytesPerWord,
	     test80xd_code.Code))
    {
    	setroute(TheLink, p->parent, p->route);
	if (!getresult(TheLink, p, logging))
	    return (FALSE);		/* give up on this one */
    } else
	AbortExit(PROGRAM_NAME, "Failed to load Transputer %d - TEST80XD", p->tpid);
    return(TRUE);
}
