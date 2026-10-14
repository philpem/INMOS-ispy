/* Host-only FTEST protocol and routing-failure fixture. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "inmos.h"
#include "checklib.h"
#include "cklib.h"
#include "testcode.h"

static struct tpstats nodes[2];
static const char *scenario;
static int packet_number;
static int fault_stage;
static int analysed;

void *Malloc(int size) { return calloc(1, (size_t) size); }
void AbortExit(char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    exit(1);
}
int lex(FILE *input, struct tpstats **root, char *pipe, char *name, char *banner)
{
    scenario = getenv("FTEST_SCENARIO");
    if (scenario == NULL) scenario = "pass";
    nodes[0].next = &nodes[1];
    nodes[0].tptype = nodes[1].tptype = T_805 + 6;
    if (getenv("FTEST_TYPE") != NULL)
        nodes[0].tptype = nodes[1].tptype = atoi(getenv("FTEST_TYPE"));
    nodes[1].tpid = 1;
    nodes[1].parent = &nodes[0];
    nodes[1].routelen = 1;
    strcpy(nodes[0].info, "0 T805g-25");
    strcpy(nodes[1].info, "1 T805g-25");
    strcpy(pipe, "Using mock ispy");
    strcpy(name, "mock");
    strcpy(banner, "# Test network");
    *root = nodes;
    return 2;
}
LINK OpenLink(char *name) { return 1; }
int TestError(LINK link) { return 0; }
int bpw(int type) { return 4; }
int AnalyseLink(LINK link)
{
    analysed = TRUE;
    fputs("ANALYSE ROOT\n", stderr);
    return 1;
}
int WriteLink(LINK link, unsigned char *buffer, unsigned int count, int timeout)
{
    if (!analysed || count != (unsigned int) buffer[0] + 1) abort();
    fputs("SEND STATE BOOTSTRAP\n", stderr);
    return (int) count;
}
int CloseLink(LINK link)
{
    free(nodes[0].extra);
    free(nodes[1].extra);
    return 1;
}
void setroute(LINK link, struct tpstats *node, int lastlink)
{
    if (node != NULL && node->tpid == 1)
        fputs("VISIT PROCESSOR 1\n", stderr);
}
int load(LINK link, struct tpstats *node, long size, long offset,
         long workspace, long vectors, int bpw, unsigned char *code)
{
    struct testinfo *q = (struct testinfo *) node->extra;
    if (!strcmp(q->stage, "ROUTE16") || !strcmp(q->stage, "ROUTE32")) {
        fprintf(stderr, "RESTORE ROUTER %d\n", node->tpid);
    } else {
        fprintf(stderr, "LOAD %s %d\n", q->stage, node->tpid);
        packet_number = 0;
        fault_stage = node->tpid == 0 && !strcmp(q->stage, "TEST85CM");
        if (!strcmp(scenario, "probe-timeout"))
            fault_stage = node->tpid == 0 && !strcmp(q->stage, "PROBE85");
    }
    if (node->tpid == 0 &&
        ((!strcmp(scenario, "load-fail") && !strcmp(q->stage, "TEST85CM")) ||
         (!strcmp(scenario, "restore-fail") && !strcmp(q->stage, "ROUTE32"))))
        return FALSE;
    return TRUE;
}
int getiserver(LINK link, unsigned int *length, unsigned char *buffer, unsigned int maximum)
{
    const char *text = ".";
    if (analysed) {
        static const unsigned char snapshot[32] = {
            1,0,0,0, 0x78,0x56,0x34,0x12, 1,1,0,0x80, 0x10,0,0,0x80,
            0,0,0,0x80, 0,0,0,0x80, 0,0,0,0x80, 0,0,0,0x80
        };
        if (!strcmp(scenario, "analyse-short")) {
            *length = 1;
            buffer[0] = 0;
        } else {
            *length = sizeof(snapshot);
            if (maximum < *length) abort();
            memcpy(buffer, snapshot, sizeof(snapshot));
        }
        return TRUE;
    }
    if (fault_stage) {
        if (!strcmp(scenario, "timeout") || !strcmp(scenario, "probe-timeout") || !strcmp(scenario, "analyse-short")) {
            /* A failure can leave an oversized/stale length. Never index it. */
            *length = 60000;
            return FALSE;
        }
        if (!strcmp(scenario, "empty")) { *length = 0; return TRUE; }
        if (!strcmp(scenario, "malformed")) text = "unexpected";
        if (!strcmp(scenario, "fail") && packet_number == 0) text = "?Conditional Mux";
        if (!strcmp(scenario, "fail-before-skip") && packet_number == 0) text = "?Integer ALU";
        if (!strcmp(scenario, "checkpoint") && packet_number == 0) text = ":BEGIN Conditional Mux";
        if (!strcmp(scenario, "long-errors") && packet_number < 20) {
            *length = 127;
            if (maximum < *length) abort();
            memset(buffer, 'x', *length);
            buffer[0] = '?';
            packet_number++;
            return TRUE;
        }
        if (!strcmp(scenario, "long-progress") && packet_number == 0) {
            *length = 127;
            if (maximum < *length) abort();
            memset(buffer, 'x', *length);
            buffer[0] = ' ';
            packet_number++;
            return TRUE;
        }
    }
    *length = (unsigned int) strlen(text);
    if (maximum < *length) abort();
    memcpy(buffer, text, *length);
    packet_number++;
    return TRUE;
}
static int test_stage(LINK link, struct tpstats *node, char logging, const char *name)
{
    struct testinfo *info = (struct testinfo *) node->extra;
    info->stage = name;
    info->tested = TRUE;
    packet_number = 0;
    fault_stage = node->tpid == 0 && !strcmp(name, "TEST801A");
    if (!strcmp(scenario, "fail-before-skip"))
        fault_stage = node->tpid == 0 && !strcmp(name, "TESTT32A");
    return getresult(link, node, logging);
}
int testT2(LINK link, struct tpstats *node, char log) { return test_stage(link, node, log, "TEST222A"); }
int testT32(LINK link, struct tpstats *node, char log) { return test_stage(link, node, log, "TESTT32A"); }
int testT414(LINK link, struct tpstats *node, char log) { return test_stage(link, node, log, "TEST414A"); }
int testT425(LINK link, struct tpstats *node, char log) { return test_stage(link, node, log, "TEST425A"); }
int testT80x(LINK link, struct tpstats *node, char log) { return test_stage(link, node, log, "TEST80XA"); }
int testT800(LINK link, struct tpstats *node, char log) { return test_stage(link, node, log, "TEST800A"); }
int testT801(LINK link, struct tpstats *node, char log) { return test_stage(link, node, log, "TEST801A"); }
