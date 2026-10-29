#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>
#include "inmos.h"
#include "linkio.h"
#include "checklib.h"
#include "cklib.h"
#include "size16.h"
#include "size32.h"
#include "hardware.h"

extern LINK TheLink;
extern struct tpstats *root;
extern char LinkName[64];
extern void check(int, int, int, int, int);
static struct hardware *output;
static int (*pump)(void);
static int active, opened;
static const char *current_stage = "idle";
int hardware_fault_stall, hardware_fault_read_blocked;
static jmp_buf escape;
struct allocation { struct allocation *next; double align; };
static struct allocation *allocations;

void hw_abort(char *program, ...)
{
    va_list args;
    char *format;
    (void)program;
    va_start(args, program);
    format = va_arg(args, char *);
    /* Legacy messages are fixed format strings with short bounded arguments.
     * Norcroft C5 lacks snprintf. Keep the large formatter off the UI stack. */
    {
        static char message[1024];
        vsprintf(message, format, args);
        strncpy(output->error, message, sizeof(output->error)-1);
        output->error[sizeof(output->error)-1] = 0;
    }
    va_end(args);
    longjmp(escape, 1);
}
void hw_exit(int status) { (void)status; hw_abort("Ispy", "Unexpected CLI exit"); }
void *hw_alloc(int size)
{
    struct allocation *a;
    if (size < 0) hw_abort("Ispy", "Invalid allocation");
    a = calloc(1, sizeof(*a)+(unsigned int)size);
    if (!a) hw_abort("Ispy", "Out of memory");
    a->next = allocations; allocations = a;
    return a+1;
}
void hw_free(void *pointer)
{
    struct allocation **p;
    if (!pointer) return;
    for (p = &allocations; *p; p = &(*p)->next)
        if ((void *)(*p+1) == pointer) {
            struct allocation *a = *p;
            *p = a->next; free(a); return;
        }
    hw_abort("Ispy", "Invalid diagnostic allocation");
}
void hw_service(void)
{
    if (!active) return;
    if (pump && pump()) hw_abort("Ispy", "Cancelled; rediscover before continuing");
}
const char *hardware_stage(void) { return current_stage; }
int hw_printf(const char *format, ...) { (void)format; return 0; }
int hw_fprintf(FILE *stream, const char *format, ...)
{ (void)stream; (void)format; return 0; }
int hw_puts(const char *text, FILE *stream) { (void)text; (void)stream; return 0; }
int hw_putc(int c, FILE *stream) { (void)stream; return c; }
int hw_putchar(int c) { return c; }
int hw_flush(FILE *stream) { (void)stream; return 0; }

static int node_index(struct tpstats *target)
{
    struct tpstats *p;
    int i;
    for (p = root, i = 0; p; p = p->next, ++i) if (p == target) return i;
    return -1;
}
static void snapshot(void)
{
    struct tpstats *p;
    int i, port;
    output->count = output->edge_count = 0;
    for (p = root; p; p = p->next) {
        struct graph_node *n;
        struct tpstats *ancestor;
        int depth;
        if (output->count == HW_NODES) hw_abort("Ispy", "Network exceeds 64-node spike limit");
        if (!bpw(p->tptype)) hw_abort("Ispy", "This spike cannot display C004 controllers yet");
        i = output->count++;
        n = &output->nodes[i]; memset(n, 0, sizeof(*n));
        if (p->routelen >= HW_NODES) hw_abort("Ispy", "Route exceeds spike limit");
        memset(output->routes[i], 0, HW_NODES);
        for (ancestor = p, depth = p->routelen; depth > 0; ancestor = ancestor->parent)
            output->routes[i][--depth] = (unsigned char)(ancestor->route+1);
        n->number = p->tpid; n->mhz = p->procspeed;
        if (class(p->tptype) == T_805) n->internal_bytes = 4096;
        strncpy(output->types[i], tptostr(p->tptype), 15);
        output->types[i][15] = 0; n->type = output->types[i];
    }
    for (p = root, i = 0; p; p = p->next, ++i)
        for (port = 0; port < 4; ++port) {
            int peer = node_index(p->links[port]);
            struct graph_edge *e;
            if (peer >= 0 && (peer < i || (peer == i && p->linkno[port] < port))) continue;
            if (peer < 0 && p->linkno[port] != HOST_TAG) continue;
            if (output->edge_count == HW_EDGES) hw_abort("Ispy", "Too many connections");
            e = &output->edges[output->edge_count++];
            if (peer < 0) { e->a = -1; e->aport = -1; e->b = i; e->bport = port; }
            else { e->a = i; e->aport = port; e->b = peer; e->bport = p->linkno[port]; }
        }
}
static unsigned long little32(const unsigned char *p)
{
    return (unsigned long)p[0] | ((unsigned long)p[1]<<8) |
           ((unsigned long)p[2]<<16) | ((unsigned long)p[3]<<24);
}
static void memory_count(int only, unsigned int ceiling)
{
    struct tpstats *p;
    unsigned char parameters[8], reply[16];
    int pending = 0, i, j;
    current_stage = "load RAM workers";
    /* CHECK and sparse workers share routing. Load in ascending depth, so an
     * already replaced parent still forwards boot packets to its children. */
    for (i = 0; i < HW_NODES; ++i)
        for (p = root; p; p = p->next) if (p->routelen == i && (only < 0 || p->tpid == only)) {
            int ok;
            if (bpw(p->tptype) == 2) {
                for (j = 0; j < 4; ++j)
                    if (class(p->linkno[j]) == C4) hw_abort("Ispy", "T2 controller processors are excluded from RAM probing");
                ok = tryroute(TheLink, p, TAG_HALT) && load(TheLink, p,
                    size16_code.CodeSize, size16_code.Offset, size16_code.WorkSpace,
                    size16_code.VectorSpace, size16_code.BytesPerWord, size16_code.Code);
            } else ok = tryroute(TheLink, p, TAG_HALT) && load(TheLink, p,
                    size32_code.CodeSize, size32_code.Offset, size32_code.WorkSpace,
                    size32_code.VectorSpace, size32_code.BytesPerWord, size32_code.Code);
            if (!ok) hw_abort("Ispy", "Failed loading sparse RAM worker");
        }
    current_stage = "start RAM workers";
    for (p = root; p; p = p->next) if (only < 0 || p->tpid == only) {
        unsigned long end = ceiling;
        if (bpw(p->tptype) == 2 && end > 64) end = 64;
        end = end*1024/bpw(p->tptype)-1;
        parameters[0] = (unsigned char)(p->tpid & 255);
        parameters[1] = (unsigned char)(p->tpid >> 8);
        parameters[2] = 4 | 128; /* Sparse sizing plus two target-side timing samples. */
        parameters[3] = (unsigned char)(output->nodes[node_index(p)].internal_bytes/1024);
        for (i = 4; i < 8; ++i) { parameters[i] = end & 255; end >>= 8; }
        if (!tryroute(TheLink, p, 4) || !sendiserver(TheLink, 8, parameters))
            hw_abort("Ispy", "Failed starting RAM count");
        output->nodes[node_index(p)].ram_state = 1; ++pending;
    }
    if (!pending) hw_abort("Ispy", "Selected processor is absent");
    hardware_fault_read_blocked = hardware_fault_stall;
    current_stage = "read RAM replies";
    while (pending) {
        unsigned int length;
        struct graph_node *n;
        hw_service();
        if (!getiserver(TheLink, &length, reply, sizeof(reply)) || length != 16)
            hw_abort("Ispy", "Incomplete RAM reply; completed results retained");
        p = find(root, reply[0]+((int)reply[1]<<8));
        if (!p || reply[2] > 6 || (reply[3] > 2 && reply[3] < 254))
            hw_abort("Ispy", "Invalid RAM reply");
        n = &output->nodes[node_index(p)];
        if (n->ram_state != 1) hw_abort("Ispy", "Unexpected RAM reply");
        if (little32(reply+8) && (reply[3] == 0 || reply[3] == 2)) {
            unsigned long elapsed = little32(reply+8);
            unsigned long accesses = bpw(p->tptype) == 2 ? 2048 : 1024;
            int cycles;
            if (elapsed > 65535) hw_abort("Ispy", "Invalid memory timing reply");
            /* High-priority timer ticks are microseconds. A 2 KiB MOVE reads
             * and writes each word. Round to a cycle, allowing timer and MHz
             * quantisation; this is an average, not a bus-configuration read. */
            cycles = (int)((elapsed*p->procspeed+accesses/2)/accesses);
            if (little32(reply+4)*(unsigned long)bpw(p->tptype) < n->internal_bytes)
                n->internal_cycles = cycles;
            else n->external_cycles = cycles;
        }
        /* Workers report the address of the last valid 2 KiB probe. The
         * reported block is part of the detected range, as in MTEST. */
        if (reply[3] == 0 || reply[3] == 2) {
            unsigned long limit = ceiling;
            if (bpw(p->tptype) == 2 && limit > 64) limit = 64;
            if (little32(reply+4) >= limit*1024/bpw(p->tptype))
                hw_abort("Ispy", "RAM reply exceeds probe ceiling");
            n->ram_bytes = little32(reply+4)*bpw(p->tptype)+2048;
        }
        if (little32(reply+12)) n->ram_errors = 1;
        if (reply[3]) {
            n->ram_state = reply[3] == 2 ? 2 : reply[3] == 255 ? 3 : reply[3] == 1 ? 4 : 5;
            --pending;
        }
    }
}
int hardware_run(struct hardware *result, int count_ram, int only,
                 unsigned int ceiling_kib, int (*service)(void),
                 const struct hardware *expected)
{
    int success = 0;
    if (active) return 0;
    current_stage = "initialise";
    active = 1; output = result; pump = service; opened = 0;
    result->error[0] = 0; result->ready = 0;
    if (!setjmp(escape)) {
        if (ceiling_kib < 4 || ceiling_kib > 262144U || (ceiling_kib & 1))
            hw_abort("Ispy", "RAM ceiling must be an even number from 4 to 262144 KiB");
        hw_service();
        current_stage = "open link";
        strcpy(LinkName, "TMB"); TheLink = OpenLink(LinkName);
        if (TheLink < 0) hw_abort("Ispy", "Cannot open Transputer link");
        opened = 1; root = NULL;
        current_stage = "discover";
        check(0, 0, 0, 0, 1);
        current_stage = "copy topology";
        snapshot();
        if (!result->count) hw_abort("Ispy", "No processors found");
        result->ready = 1;
        if (only >= 0 && expected) {
            int i;
            current_stage = "validate node";
            if (result->count != expected->count || result->edge_count != expected->edge_count)
                hw_abort("Ispy", "Network changed; select the node again");
            for (i = 0; i < result->count; ++i)
                if (strcmp(result->types[i], expected->types[i]) ||
                    memcmp(result->routes[i], expected->routes[i], HW_NODES) ||
                    result->nodes[i].number != expected->nodes[i].number)
                    hw_abort("Ispy", "Node route or type changed; select it again");
            if (memcmp(result->edges, expected->edges, result->edge_count*sizeof(result->edges[0])))
                hw_abort("Ispy", "Connections changed; select the node again");
        }
        if (count_ram) memory_count(only, ceiling_kib);
        success = 1;
    }
    if (opened) CloseLink(TheLink);
    while (allocations) hw_free(allocations+1);
    root = NULL; pump = NULL; active = 0; hardware_fault_read_blocked = 0;
    return success;
}
void hardware_graph(struct hardware *hardware, struct graph *graph)
{
    graph->nodes = hardware->nodes; graph->node_count = hardware->count;
    graph->edges = hardware->edges; graph->edge_count = hardware->edge_count;
    graph->sample = 0;
    graph_layout(graph, graph->width);
}
