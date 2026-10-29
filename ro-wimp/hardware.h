#ifndef ISPY_HARDWARE_H
#define ISPY_HARDWARE_H
#include "graph.h"
#define HW_NODES GRAPH_MAX_NODES
#define HW_EDGES (HW_NODES*4)
struct hardware {
    struct graph_node nodes[HW_NODES];
    struct graph_edge edges[HW_EDGES];
    char types[HW_NODES][16];
    int count, edge_count, ready;
    unsigned char routes[HW_NODES][HW_NODES];
    char error[256];
};
/* The callback services WIMP events and returns nonzero to cancel. No other
 * hardware operation may start until this call returns. Result data survives
 * errors; a failed discovery never publishes a partial topology. */
int hardware_run(struct hardware *result, int count_ram, int only,
                 unsigned int ceiling_kib, int (*service)(void),
                 const struct hardware *expected);
void hardware_graph(struct hardware *hardware, struct graph *graph);
/* Last operation stage, retained on failure for diagnostic logs. */
const char *hardware_stage(void);
/* Development fault injection, used only by --hardware smoke runs. Suppress
 * readiness after worker startup to exercise the native stalled-read loop. */
extern int hardware_fault_stall, hardware_fault_read_blocked;
#endif
