/* Hardware-description exports from an ISPY snapshot. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "inmos.h"
#include "checklib.h"
#include "cklib.h"

char *PROGRAM_NAME = "netcfg";

enum { CONFIG_C, CONFIG_OCCAM, CONFIG_SWITCH };

static void type_name(struct tpstats *p, char *name)
{
    size_t n;
    switch (class(p->tptype)) {
    case T_212: strcpy(name, "T212"); return;
    case M212: strcpy(name, "M212"); return;
    case T_225: case T_414: case T_400: case T_425:
    case T_800: case T_801: case T_805: break;
    default: AbortExit(PROGRAM_NAME, "processor %d has no supported compiler type", p->tpid);
    }
    strcpy(name, tptostr(p->tptype));
    n = strlen(name);
    if (n && islower((unsigned char)name[n-1])) name[--n] = '\0';
    if (name[0] != 'T' || n < 2 || strchr(name, '#') || strchr(name, ' '))
        AbortExit(PROGRAM_NAME, "processor %d has no supported compiler type", p->tpid);
}

static void validate(struct tpstats *root, int format)
{
    struct tpstats *p, *q;
    char name[32];
    int i, k, hosts = 0;
    for (p = root; p; p = p->next) {
        if (p->tpid < 0 || p->routelen < 0)
            AbortExit(PROGRAM_NAME, "invalid node %d", p->tpid);
        if (class(p->tptype) == C4) continue;
        if (format != CONFIG_SWITCH) type_name(p, name);
        for (i = 0; i < 4; i++) {
            k = p->linkno[i]; q = p->links[i];
            if (k == HOST_TAG) { hosts++; continue; }
            if (!q || class(q->tptype) == C4) continue;
            if (k < 0 || k > 3 || q->links[k] != p || q->linkno[k] != i)
                AbortExit(PROGRAM_NAME, "nonreciprocal connection at node%d.link[%d]", p->tpid, i);
        }
    }
    if (hosts != 1) AbortExit(PROGRAM_NAME, "expected one host connection, found %d", hosts);
}

static void switch_path(struct tpstats *p)
{
    struct tpstats *q = p;
    unsigned char *path;
    int i;
    if (p->routelen < 1 || p->routelen > 65535)
        AbortExit(PROGRAM_NAME, "invalid C004 route at node %d", p->tpid);
    path = (unsigned char *)Malloc(p->routelen);
    for (i = p->routelen; i > 0; i--) {
        if (!q || !q->parent || q->route < 0 || q->route > 3)
            AbortExit(PROGRAM_NAME, "invalid C004 route at node %d", p->tpid);
        path[i-1] = (unsigned char)q->route;
        q = q->parent;
    }
    if (!q || q->parent) AbortExit(PROGRAM_NAME, "invalid C004 root");
    for (i = 0; i < p->routelen; i++) printf(" %u", (unsigned int)path[i]);
    free(path);
}

static void emit(struct tpstats *root, int format)
{
    struct tpstats *p, *q;
    char name[32];
    int i, k;
    if (format == CONFIG_SWITCH) {
        for (p = root; p; p = p->next) if (class(p->tptype) == C4) {
            struct c4stats *c4 = (struct c4stats *)p;
            printf("switch\n  name \"c004_%d\"\n  type C004\n  path", p->tpid);
            switch_path(p);
            printf("\n  connect");
            for (i = 0; i < C004_PORTS; i++) {
                k = c4->config[i];
                if (k < C004_EMPTY || k >= C004_PORTS)
                    AbortExit(PROGRAM_NAME, "invalid C004 selection");
                if (k != C004_EMPTY) printf(" %d>%d", k, i);
            }
            printf("\n\n");
        }
        return;
    }
    if (format == CONFIG_C)
        printf("/* ISPY hardware fragment: add memory sizes and application mapping. */\n");
    else {
        printf("-- ISPY hardware section: add memory sizes and application mapping.\n");
        for (p = root; p; p = p->next) if (class(p->tptype) != C4)
            printf("NODE node%d:\n", p->tpid);
        printf("ARC hostlink:\nNETWORK\n  DO\n");
    }
    for (p = root; p; p = p->next) if (class(p->tptype) != C4) {
        type_name(p, name);
        if (format == CONFIG_C) printf("%s node%d;\n", name, p->tpid);
        else printf("    SET node%d (type := \"%s\")\n", p->tpid, name);
    }
    for (p = root; p; p = p->next) if (class(p->tptype) != C4) {
        for (i = 0; i < 4; i++) {
            k = p->linkno[i]; q = p->links[i];
            if (k == HOST_TAG) {
                if (format == CONFIG_C) printf("connect host to node%d.link[%d];\n", p->tpid, i);
                else printf("    CONNECT node%d[link][%d] TO HOST WITH hostlink\n", p->tpid, i);
            } else if (q && class(q->tptype) != C4 &&
                       (p->tpid < q->tpid || (p == q && i < k))) {
                if (format == CONFIG_C)
                    printf("connect node%d.link[%d] to node%d.link[%d];\n", p->tpid, i, q->tpid, k);
                else printf("    CONNECT node%d[link][%d] TO node%d[link][%d]\n", p->tpid, i, q->tpid, k);
            }
        }
    }
    if (format == CONFIG_OCCAM) puts(":");
}

int main(int argc, char **argv)
{
    struct tpstats *root = NULL, *p;
    FILE *input = stdin;
    char pipe[128], link[64], banner[128];
    int format = CONFIG_C, i;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h")) {
            puts("Usage: netcfg [-cc|-co|-sw] [-f snapshot]"); return 0;
        }
        if (!strcmp(argv[i], "-cc")) format = CONFIG_C;
        else if (!strcmp(argv[i], "-co")) format = CONFIG_OCCAM;
        else if (!strcmp(argv[i], "-sw")) format = CONFIG_SWITCH;
        else if (!strcmp(argv[i], "-f") && ++i < argc) {
            if (input != stdin) AbortExit(PROGRAM_NAME, "only one input file allowed");
            input = fopen(argv[i], "r");
            if (!input) AbortExit(PROGRAM_NAME, "cannot open %s", argv[i]);
        } else {
            fprintf(stderr, "Usage: netcfg [-cc|-co|-sw] [-f snapshot]\n");
            return 1;
        }
    }
    if (!lexquiet(input, &root, pipe, link, banner) || ferror(input))
        AbortExit(PROGRAM_NAME, "no usable network snapshot");
    if (input != stdin) fclose(input);
    validate(root, format);
    if (format == CONFIG_SWITCH) {
        for (p = root; p; p = p->next) if (class(p->tptype) != C4) {
            for (i = 0; i < 4; i++)
                if (class(p->linkno[i]) == C4 && p->links[i] == NULL)
                    AbortExit(PROGRAM_NAME, "C004 configuration not captured at node%d.link[%d]; use ispy -c4", p->tpid, i);
        }
    }
    emit(root, format);
    while (root) { p = root; root = p->next; free(p); }
    return fflush(stdout) == 0 ? 0 : 1;
}
