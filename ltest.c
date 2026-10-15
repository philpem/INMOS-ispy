#define _POSIX_C_SOURCE 200809L
/* Bidirectional link integrity and round-trip timing diagnostic. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef RISCOS
#include "oslib/os.h"
#else
#include <time.h>
#endif
#include "inmos.h"
#include "checklib.h"
#include "cklib.h"
#include "ltest16.h"
#include "ltest32.h"

char *PROGRAM_NAME = "ltest";
static LINK TheLink;

static unsigned long wallticks(void)
{
#ifdef RISCOS
    int ticks;
    if (xos_read_monotonic_time(&ticks) != NULL) return 0;
    return (unsigned long)(unsigned int)ticks;
#else
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return 0;
    return (unsigned long)now.tv_sec * 100UL + (unsigned long)now.tv_nsec / 10000000UL;
#endif
}

static unsigned int word(unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static unsigned int checksum(unsigned char *data, unsigned int length)
{
    unsigned int crc = 65535U, i, bit;
    for (i = 0; i < length; i++) {
        crc ^= (unsigned int)data[i] << 8;
        for (bit = 0; bit < 8; bit++)
            crc = ((crc << 1) ^ ((crc & 32768U) ? 0x1021U : 0U)) & 65535U;
    }
    return crc;
}

static int configure(struct tpstats *p, int port, int role, unsigned int rounds, int size)
{
    unsigned char config[8];
    memset(config, 0, sizeof(config));
    config[0] = (unsigned char)role;
    config[1] = (unsigned char)port;
    config[2] = (unsigned char)rounds;
    config[3] = (unsigned char)(rounds >> 8);
    config[4] = (unsigned char)size;
    if (!tryroute(TheLink, p, 4)) return FALSE;
    return sendiserver(TheLink, sizeof(config), config);
}

static int pair(struct tpstats *a, int ap, struct tpstats *b, int bp,
                unsigned int rounds, int size)
{
    unsigned char result[14];
    unsigned int length = 0, ab, ba, ticks;
    unsigned long started = wallticks(), elapsed;
    /* The receiver cannot be an ancestor of the initiator: it temporarily
     * owns its physical port and suspends forwarding until the test ends. */
    if (a->routelen > b->routelen) {
        struct tpstats *p = a;
        int port = ap;
        a = b; ap = bp; b = p; bp = port;
    }
    printf("node%d.link[%d] <-> node%d.link[%d] %s: ",
           a->tpid, ap, b->tpid, bp, size == 1 ? "latency" : "integrity");
    fflush(stdout);
    if (!configure(b, bp, 1, rounds, size) ||
        !getiserver(TheLink, &length, result, sizeof(result)) ||
        length != 1 || result[0] != 1 ||
        !configure(a, ap, 2, rounds, size) ||
        !getiserver(TheLink, &length, result, sizeof(result))) {
        puts("NOT COMPLETED (communication failure)");
        return -1;
    }
    if (length != 14 || word(result+12) != checksum(result, 12) ||
        (word(result) ^ word(result+2)) != 65535U || result[11] != 2 || word(result+8) != rounds ||
        result[10] != size) {
        puts("NOT COMPLETED (invalid result)");
        return -1;
    }
    ab = word(result); ba = word(result+4); ticks = word(result+6);
    if (ab > rounds*(unsigned int)size || ba > rounds*(unsigned int)size) {
        puts("NOT COMPLETED (invalid error counts)"); return -1;
    }
    elapsed = wallticks() - started;
    printf("%u bytes each way, errors %u/%u, ", rounds * (unsigned int)size, ab, ba);
    if (elapsed >= 400UL || started == 0) printf("timing unavailable (timer wrap possible)");
    else if (ticks == 0) printf("below timer resolution");
    else {
        double seconds = (double)ticks * 0.000064;
        if (size == 1) printf("mean round trip %.1f us", seconds * 1000000.0 / rounds);
        else printf("round-trip goodput %.3f MB/s", 2.0 * rounds * size / seconds / 1000000.0);
    }
    puts(ab || ba ? " FAIL" : " PASS");
    return ab || ba ? 1 : 0;
}

static int load_worker(struct tpstats *p)
{
    if (!tryroute(TheLink, p, TAG_HALT)) return FALSE;
    if (bpw(p->tptype) == 2)
        return load(TheLink, p, ltest16_code.CodeSize, ltest16_code.Offset,
                    ltest16_code.WorkSpace, ltest16_code.VectorSpace,
                    ltest16_code.BytesPerWord, ltest16_code.Code);
    return load(TheLink, p, ltest32_code.CodeSize, ltest32_code.Offset,
                ltest32_code.WorkSpace, ltest32_code.VectorSpace,
                ltest32_code.BytesPerWord, ltest32_code.Code);
}

int main(int argc, char **argv)
{
    FILE *input = stdin;
    struct tpstats *root = NULL, *p, *q;
    char pipe[128], link[64], banner[128], *end;
    unsigned long parsed;
    unsigned int rounds = 128;
    int i, depth, maxdepth = 0, status = 0, failed = 0, tested = 0, r;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h")) {
            puts("Usage: ltest [-f snapshot] [-n rounds (1..256)]"); return 0;
        } else if (!strcmp(argv[i], "-f") && i+1 < argc) {
            if (input != stdin) AbortExit(PROGRAM_NAME, "only one input file allowed");
            input = fopen(argv[++i], "r");
            if (!input) AbortExit(PROGRAM_NAME, "cannot open snapshot");
        } else if (!strcmp(argv[i], "-n") && i+1 < argc) {
            parsed = strtoul(argv[++i], &end, 10);
            if (!*argv[i] || *end || parsed < 1 || parsed > 256)
                AbortExit(PROGRAM_NAME, "rounds must be from 1 to 256");
            rounds = (unsigned int)parsed;
        } else AbortExit(PROGRAM_NAME, "unknown or incomplete option %s", argv[i]);
    }
    if (!lexquiet(input, &root, pipe, link, banner) || ferror(input))
        AbortExit(PROGRAM_NAME, "no usable network snapshot");
    if (input != stdin) fclose(input);
    /* Check all endpoints before loading any target. */
    for (p = root; p; p = p->next) {
        if (class(p->tptype) == C4) continue;
        if (bpw(p->tptype) != 2 && bpw(p->tptype) != 4)
            AbortExit(PROGRAM_NAME, "unsupported processor %d", p->tpid);
        if (p->routelen > maxdepth) maxdepth = p->routelen;
        for (i = 0; i < 4; i++) {
            q = p->links[i];
            if (q && class(q->tptype) != C4 &&
                (p->linkno[i] < 0 || p->linkno[i] > 3 ||
                 q->links[p->linkno[i]] != p || q->linkno[p->linkno[i]] != i))
                AbortExit(PROGRAM_NAME, "nonreciprocal link at processor %d", p->tpid);
        }
    }
    TheLink = OpenLink(link);
    if (TheLink < 0) AbortExit(PROGRAM_NAME, "cannot open link %s", link);
    for (depth = 0; depth <= maxdepth && !failed; depth++)
        for (p = root; p && !failed; p = p->next)
            if (class(p->tptype) != C4 && p->routelen == depth && !load_worker(p)) {
                fprintf(stderr, "Failed loading processor %d; no link tests started\n", p->tpid);
                failed = 1;
            }
    puts("Directional errors are ordered as the printed endpoints; timing is round-trip, including worker overhead.");
    for (p = root; p; p = p->next) if (class(p->tptype) != C4)
        for (i = 0; i < 4; i++) {
            q = p->links[i];
            if (!q || class(q->tptype) == C4 || p->tpid > q->tpid) continue;
            if (p == q && i >= p->linkno[i]) continue;
            if (failed || p == q) {
                printf("node%d.link[%d] <-> node%d.link[%d]: NOT TESTED%s\n",
                       p->tpid, i, q->tpid, p->linkno[i], p == q ? " (self loop)" : "");
                status = 1; continue;
            }
            r = pair(p, i, q, p->linkno[i], rounds, 1);
            if (r < 0) { failed = 1; status = 1; continue; }
            status |= r;
            r = pair(p, i, q, p->linkno[i], rounds, 64);
            if (r < 0) { failed = 1; status = 1; continue; }
            status |= r; tested++;
        }
    if (!tested) { puts("No processor-to-processor links completed"); status = 1; }
    CloseLink(TheLink);
    while (root) { p = root; root = p->next; free(p); }
    return status || failed ? 1 : 0;
}
