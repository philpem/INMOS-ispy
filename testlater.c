/* Later-revision INMOS diagnostics. Original test algorithms: INMOS FTEST. */
#include <stdio.h>
#include "inmos.h"
#include "checklib.h"
#include "cklib.h"
#include "testcode.h"
#include "probe85.h"
#include "test85cm.h"
#include "test85st.h"
#include "test85co.h"
#include "test85in.h"
#include "probe45.h"
#include "test45cm.h"
#include "test45st.h"
#include "test45co.h"
#include "test45in.h"

int later_supported(int type)
{
    return type == T_801 + 1 || type == T_805 + 3 || type == T_805 + 6 ||
           type == T_425 + 1 || type == T_425 + 2;
}

static int run_payload(LINK link, struct tpstats *p, char logging,
                       const char *name, long size, long entry, long workspace,
                       long vectors, int words, unsigned char *code)
{
    struct testinfo *q = (struct testinfo *) p->extra;
    q->stage = name;
    if (!load(link, p, size, entry, workspace, vectors, words, code))
        return load_failed(p);
    q->tested = TRUE;
    setroute(link, p->parent, p->route);
    return getresult(link, p, logging);
}

#define RUN(blob, name) \
    if (!run_payload(link, p, logging, name, blob##_code.CodeSize, \
                     blob##_code.Offset, blob##_code.WorkSpace, \
                     blob##_code.VectorSpace, blob##_code.BytesPerWord, \
                     blob##_code.Code)) return FALSE

int testLater(LINK link, struct tpstats *p, char logging, int mode)
{
    if (!later_supported(p->tptype))
        AbortExit("ftest", "No later-revision payload for processor %d", p->tpid);
    if (class(p->tptype) == T_425) {
        if (mode == -1) {
            RUN(probe45, "PROBE45");
        } else {
            if (mode == 0 || mode == 1) { RUN(test45cm, "TEST45CM"); }
            if (mode == 0 || mode == 2) { RUN(test45st, "TEST45ST"); }
            if (mode == 0 || mode == 3) { RUN(test45co, "TEST45CO"); }
            if (mode == 0 || mode == 4) { RUN(test45in, "TEST45IN"); }
        }
    } else {
        if (mode == -1) {
            RUN(probe85, "PROBE85");
        } else {
            if (mode == 0 || mode == 1) { RUN(test85cm, "TEST85CM"); }
            if (mode == 0 || mode == 2) { RUN(test85st, "TEST85ST"); }
            if (mode == 0 || mode == 3) { RUN(test85co, "TEST85CO"); }
            if (mode == 0 || mode == 4) { RUN(test85in, "TEST85IN"); }
        }
    }
    return TRUE;
}
