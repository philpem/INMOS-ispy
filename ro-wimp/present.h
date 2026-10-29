#ifndef ISPY_PRESENT_H
#define ISPY_PRESENT_H
#include "graph.h"
/* Text buffers must hold at least 80 bytes. Internal capacity is type-derived;
 * the external figure is a probe bound/period, not a usable-memory sum. */
unsigned long node_external_bytes(const struct graph_node *node);
void node_ram_label(const struct graph_node *node, char *text);
void node_ram_value(const struct graph_node *node, int external, char *text);
#endif
