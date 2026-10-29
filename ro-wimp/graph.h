#ifndef ISPY_GRAPH_H
#define ISPY_GRAPH_H

#define GRAPH_MAX_NODES 64
#define GRAPH_MAX_EDGES (GRAPH_MAX_NODES*4)
#define GRAPH_EXAMPLES 8
enum { GRAPH_AUTO, GRAPH_LAYER_MODE, GRAPH_PREVIOUS };
enum { GRAPH_EMPTY, GRAPH_COMPACT, GRAPH_PIPELINE, GRAPH_RING, GRAPH_GRID,
       GRAPH_LAYERED, GRAPH_BASELINE, GRAPH_FIXED };

struct graph_box { int x0, y0, x1, y1; };
struct graph_node {
    int number;
    const char *type;
    int mhz;
    struct graph_box box;
    int selected;
    unsigned long ram_bytes;
    int ram_state; /* 0 untested, 1 incomplete, 2 ceiling, 3 wrap, 4 probe fail, 5 alias */
    int ram_errors;
    unsigned long internal_bytes; /* Processor-type internal RAM capacity, or 0 if unknown. */
    int internal_cycles, external_cycles; /* Estimated cycles per word access; 0 unknown. */
};
struct graph_edge {
    int a, aport, b, bport; /* -1 denotes the host. */
};
struct graph_point { int x, y; };
struct graph_path {
    struct graph_point points[8], label_a, label_b;
    int count;
};
struct graph {
    struct graph_node *nodes;
    int node_count;
    const struct graph_edge *edges;
    int edge_count;
    int sample;
    int width;
    int height;
    int layout_mode, layout_kind, host_y;
    int tracks[GRAPH_MAX_EDGES];
};

void quality_place(struct graph *graph, int width);
const char *graph_layout_name(const struct graph *graph);
int graph_example(struct graph *graph, int example);
const char *graph_example_name(int example);
void graph_empty(struct graph *graph);
void graph_sample(struct graph *graph);
void graph_sample_grid(struct graph *graph);
void graph_sample_layered(struct graph *graph);
int graph_node_y(const struct graph_node *node);
void graph_edge_path(const struct graph *graph, int edge, struct graph_path *path);
void graph_select(struct graph *graph, int index, int adjust);
void graph_layout(struct graph *graph, int width);
int graph_hit(const struct graph *graph, int x, int y);
/* Screen coordinates to WIMP work-area coordinates (including scrolling). */
int graph_pointer(const struct graph *graph, int x, int y,
                  int visible_x0, int visible_y1, int xscroll, int yscroll);

#endif
