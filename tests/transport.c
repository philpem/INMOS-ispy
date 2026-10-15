#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "inmos.h"
#include "checklib.h"
#include "cklib.h"
char *PROGRAM_NAME = "transport-test";
static int mode;
static unsigned char wire[2048];
static unsigned int used;
LINK OpenLink(char *name) { return 0; }
int CloseLink(LINK link) { return 1; }
int ResetLink(LINK link) { return 1; }
int AnalyseLink(LINK link) { return 1; }
int TestError(LINK link) { return 0; }
int ReadLink(LINK link, unsigned char *buf, unsigned int count, int timeout)
{
    if (mode == 1) return -1;
    if (mode == 2) return (int)count+1;
    memset(buf, 255, count);
    return (int)count;
}
int WriteLink(LINK link, unsigned char *buf, unsigned int count, int timeout)
{
    if (mode == 1) return -1;
    if (mode == 2) return (int)count+1;
    if (mode == 3) count = 1;
    if (count > sizeof(wire)-used) abort();
    memcpy(wire+used, buf, count); used += count;
    return (int)count;
}
int main(int argc, char **argv)
{
    struct tpstats node;
    unsigned char buf[700];
    unsigned int i, length = 123;
    mode = argc > 2 ? atoi(argv[2]) : 0;
    memset(&node, 0, sizeof(node));
    node.tpid = 42;
    for (i = 0; i < sizeof(buf); i++) buf[i] = (unsigned char)i;
    if (!strcmp(argv[1], "receive")) {
        if (getiserver(0, &length, buf, sizeof(buf))) return 2;
        printf("length %u\n", length);
    } else if (!strcmp(argv[1], "packet")) {
        printf("sent %d bytes %u\n", sendiserver(0, mode == 4 ? 65536U : 32U, buf), used);
    } else if (!strcmp(argv[1], "route")) {
        setroute(0, &node, 4);
        printf("route %u\n", used);
    } else if (!strcmp(argv[1], "load")) {
        int ok;
        #include "boot.h"
        ok = load(0, &node, sizeof(buf), 3, 100, 0, 4, buf);
        if (!ok) return 1;
        if (used != sizeof(boot)+16+sizeof(buf) ||
            memcmp(wire, boot, sizeof(boot)) ||
            memcmp(wire+sizeof(boot)+16, buf, sizeof(buf))) return 2;
        puts("loaded exact stream");
    }
    return 0;
}
