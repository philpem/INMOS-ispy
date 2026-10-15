/* Host-workflow doubles, separate from the compiled-payload executor. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "inmos.h"
#include "checklib.h"
#include "cklib.h"
static struct tpstats nodes[3];
static struct tpstats *selected, *receiver;
static unsigned char config[8];
static int receiver_port, calls, next_result;
static unsigned long ceilings[3];
static const char *scenario(void)
{
    const char *s = getenv("DIAG_CASE"); return s ? s : "pass";
}
void AbortExit(char *first, ...)
{
    va_list ap;
    char *format;
    va_start(ap, first);
    fprintf(stderr, "%s: ", first);
    format = va_arg(ap, char *);
    vfprintf(stderr, format, ap);
    va_end(ap); fputc('\n', stderr); exit(1);
}
void *Malloc(int size) { void *p = calloc(1, size); if (!p) abort(); return p; }
int bpw(int type) { return type == T_212 ? 2 : 4; }
struct tpstats *find(struct tpstats *root, int id)
{
    for (; root; root = root->next) if (root->tpid == id) return root;
    return NULL;
}
int lex(FILE *f, struct tpstats **root, char *pipe, char *link, char *banner)
{
    int i;
    memset(nodes, 0, sizeof(nodes));
    for (i = 0; i < 3; i++) {
        struct tpstats *p = &nodes[i];
        p->tpid = i; p->tptype = T_425; p->routelen = i;
        p->procspeed = 20;
        p->parent = i ? &nodes[i-1] : NULL;
        p->next = i < 2 ? &nodes[i+1] : NULL;
        p->bootlink = i ? 1 : 0;
        p->linkno[0] = p->linkno[1] = p->linkno[2] = p->linkno[3] = TXXX;
        sprintf(p->info, "%d T425a-20 1.0M %d [ ... ... ... ... ]", i, p->bootlink);
    }
    nodes[0].linkno[0] = HOST_TAG;
    nodes[0].links[1] = &nodes[1]; nodes[0].linkno[1] = 1;
    nodes[1].links[1] = &nodes[0]; nodes[1].linkno[1] = 1;
    nodes[1].links[2] = &nodes[2]; nodes[1].linkno[2] = 1;
    nodes[2].links[1] = &nodes[1]; nodes[2].linkno[1] = 2;
    nodes[0].links[2] = &nodes[2]; nodes[0].linkno[2] = 2;
    nodes[2].links[2] = &nodes[0]; nodes[2].linkno[2] = 2;
    /* Diagnostics free snapshots, so give them an actual allocated graph. */
    {
        struct tpstats *copy[3];
        int j;
        for (i = 0; i < 3; i++) { copy[i] = Malloc(sizeof(*copy[i])); *copy[i] = nodes[i]; }
        for (i = 0; i < 3; i++) {
            copy[i]->parent = i ? copy[i-1] : NULL;
            copy[i]->next = i < 2 ? copy[i+1] : NULL;
            for (j = 0; j < 4; j++)
                if (copy[i]->links[j]) copy[i]->links[j] = copy[copy[i]->links[j]-nodes];
        }
        *root = copy[0];
    }
    strcpy(pipe, "Using mock ispy"); strcpy(link, "mock"); strcpy(banner, "# Network");
    return 3;
}
int lexquiet(FILE *f, struct tpstats **r, char *p, char *l, char *b) { return lex(f,r,p,l,b); }
LINK OpenLink(char *name) { fprintf(stderr, "OPEN LINK\n"); return 0; }
int CloseLink(LINK link) { return 1; }
int tryroute(LINK link, struct tpstats *p, int last)
{
    selected = p;
    return strcmp(scenario(), "route-fail") != 0;
}
void setroute(LINK link, struct tpstats *p, int last)
{
    if (!tryroute(link,p,last)) AbortExit("mock", "route failed");
}
int load(LINK l, struct tpstats *p, long cs, long o, long ws, long vs, int bp, unsigned char *code)
{
    fprintf(stderr, "LOAD %d %ld\n", p->tpid, cs);
    return !(p->tpid == 1 && !strcmp(scenario(), "load-fail"));
}
int sendiserver(LINK l, unsigned int length, unsigned char *data)
{
    int i;
    if (!strcmp(scenario(), "send-fail")) return 0;
    if (length != 8) abort();
    memcpy(config, data, 8);
#ifdef MEMORY_TEST
    for (i = 7; i >= 4; i--) ceilings[selected->tpid] = (ceilings[selected->tpid]<<8) | data[i];
#else
    (void)i;
    if (config[0] == 1) { receiver = selected; receiver_port = config[1]; }
    else {
        if (!receiver || selected->links[config[1]] != receiver ||
            receiver->links[receiver_port] != selected || selected->routelen > receiver->routelen) abort();
        fprintf(stderr, "PAIR %d:%d %d:%d\n", selected->tpid, config[1], receiver->tpid, receiver_port);
    }
#endif
    return 1;
}
int getiserver(LINK l, unsigned int *length, unsigned char *data, unsigned int max)
{
    calls++;
    *length = 0;
    if (!strcmp(scenario(), "timeout") || (calls == 5 && !strcmp(scenario(), "later-timeout"))) return 0;
    memset(data, 0, max);
#ifdef MEMORY_TEST
    {
        unsigned long addr = ceilings[next_result]/512UL*512UL;
        int i;
        data[0] = (unsigned char)next_result++;
        data[2] = 6;
        data[3] = !strcmp(scenario(), "alias") ? 254 : 2;
        for (i = 4; i < 8; i++) { data[i] = (unsigned char)addr; addr >>= 8; }
        *length = 16;
    }
#else
    if (config[0] == 1) {
        *length = 1; data[0] = !strcmp(scenario(), "bad-ready") ? 0 : 1;
    } else {
        unsigned int crc = 65535U, i, bit;
        *length = !strcmp(scenario(), "short") ? 13 : 14;
        data[0] = !strcmp(scenario(), "corrupt") ? 1 : 0;
        data[2] = (unsigned char)~data[0]; data[3] = 255;
        data[4] = !strcmp(scenario(), "corrupt") ? 2 : 0;
        data[6] = 100;
        data[8] = config[2]; data[9] = config[3]; data[10] = config[4]; data[11] = 2;
        for (i = 0; i < 12; i++) {
            crc ^= (unsigned int)data[i] << 8;
            for (bit = 0; bit < 8; bit++)
                crc = ((crc << 1) ^ ((crc & 32768U) ? 0x1021U : 0U)) & 65535U;
        }
        data[12] = (unsigned char)crc; data[13] = (unsigned char)(crc >> 8);
        if (!strcmp(scenario(), "bad-checksum")) data[0] ^= 1;
        if (!strcmp(scenario(), "bad-counter")) data[2] ^= 1;
    }
#endif
    return 1;
}
