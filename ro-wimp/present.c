#include <stdio.h>
#include <string.h>
#include "present.h"

unsigned long node_external_bytes(const struct graph_node *node)
{
    return node->ram_bytes > node->internal_bytes ?
           node->ram_bytes-node->internal_bytes : 0;
}

void node_ram_label(const struct graph_node *node, char *text)
{
    unsigned long kib = node_external_bytes(node)/1024;
    int lower = node->ram_state == 1 || node->ram_state == 2 || node->ram_state == 4;
    if (!node->ram_state) strcpy(text, "RAM: --");
    else if (node->ram_errors) strcpy(text, "RAM errors");
    else if (node->ram_state == 5) strcpy(text, "Unknown");
    else if (!node->ram_bytes) strcpy(text, "Incomplete");
    else if (node->internal_bytes) {
        sprintf(text, "%lu/%s%lu KiB", node->internal_bytes/1024,
                lower ? ">=" : "", kib);
        /* The centred graph field holds eleven desktop-font characters.
         * Keep both banks and any lower-bound qualifier on large results. */
        if (strlen(text) > 11)
            sprintf(text, "%lu/%s%luK", node->internal_bytes/1024,
                    lower ? ">=" : "", kib);
    }
    else if (lower && kib >= 100000)
        sprintf(text, ">=%lu MiB", kib/1024);
    else sprintf(text, "%s%lu KiB", lower ? ">=" : "", kib);
}

void node_ram_value(const struct graph_node *node, int external, char *text)
{
    unsigned long kib = node_external_bytes(node)/1024;
    if (!external) {
        /* Currently populated only for T805: fall back to its specified
         * one-cycle access when the scan has not measured a timing. */
        if (node->internal_bytes) {
            int cycles = node->internal_cycles ? node->internal_cycles : 1;
            sprintf(text, "%lu KiB, %d cycle%s", node->internal_bytes/1024,
                    cycles, cycles == 1 ? "" : "s");
        }
        else strcpy(text, "Unknown");
    } else if (!node->ram_state) strcpy(text, "Not counted");
    else if (node->ram_state == 5) strcpy(text, "Unknown (alias detected)");
    else if (node->ram_errors) strcpy(text, "Memory errors detected");
    else if (!node->ram_bytes) strcpy(text, "Incomplete");
    else if (node->ram_state == 2) sprintf(text, ">=%lu KiB (ceiling)", kib);
    else if (node->ram_state == 1) sprintf(text, ">=%lu KiB (partial)", kib);
    else if (node->ram_state == 4) sprintf(text, ">=%lu KiB (failed)", kib);
    else if (node->external_cycles)
        sprintf(text, "%lu KiB, %d cycle%s", kib, node->external_cycles,
                node->external_cycles == 1 ? "" : "s");
    else sprintf(text, "%lu KiB, cycles unknown", kib);
}
