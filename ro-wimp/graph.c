#include "graph.h"
#include <string.h>

/* Demonstration data only. No hardware has been examined. */
static struct graph_node sample_nodes[] = {
    {0, "T805G", 25, {400, -330, 640, -150}, 0, 0, 0, 0, 0, 0, 0},
    {1, "T805G", 25, {840, -330, 1080, -150}, 0, 0, 0, 0, 0, 0, 0}
};
static const struct graph_edge sample_edges[] = {
    {-1, -1, 0, 0}, {0, 1, 1, 2}, {0, 2, 1, 1}
};
static struct graph_node grid_nodes[9];
static struct graph_edge grid_edges[13];

void graph_empty(struct graph *graph)
{
    memset(graph, 0, sizeof(*graph));
    graph->width = 1400;
    graph->height = 440;
    graph->host_y = -210;
}

void graph_sample_grid(struct graph *graph)
{
    int i, row, col, edges = 0;
    graph_empty(graph);
    memset(grid_nodes, 0, sizeof(grid_nodes));
    grid_edges[edges].a = -1; grid_edges[edges].aport = -1;
    grid_edges[edges].b = 0; grid_edges[edges++].bport = 0;
    for (i = 0; i < 9; ++i) {
        struct graph_node *n = &grid_nodes[i];
        row = i/3; col = i%3;
        n->number = i; n->type = "T805G"; n->mhz = 25;
        n->internal_bytes = 4096; n->ram_bytes = 4198400UL;
        n->ram_state = 3; n->internal_cycles = 1; n->external_cycles = 3;
        if (col < 2) {
            grid_edges[edges].a = i; grid_edges[edges].aport = 1;
            grid_edges[edges].b = i+1; grid_edges[edges++].bport = 3;
        }
        if (row < 2) {
            grid_edges[edges].a = i; grid_edges[edges].aport = 2;
            grid_edges[edges].b = i+3; grid_edges[edges++].bport = 0;
        }
    }
    graph->nodes = grid_nodes; graph->node_count = 9;
    graph->edges = grid_edges; graph->edge_count = edges;
    graph->sample = 2;
    graph_layout(graph, 960);
}

void graph_sample_layered(struct graph *graph)
{
    graph_sample_grid(graph);
    graph->sample = 3;
    graph->layout_mode = GRAPH_LAYER_MODE;
    graph_layout(graph, 752);
}

/* Shortest undirected hop distance from the host; use every physical link,
 * including links outside the discovery tree. Each node enters the queue once.
 * Disconnected components follow the reachable component in separate columns. */
static void baseline_layout(struct graph *graph, int width)
{
    int depth[GRAPH_MAX_NODES], queue[GRAPH_MAX_NODES];
    int rows[GRAPH_MAX_NODES], links[GRAPH_MAX_NODES], columns[GRAPH_MAX_NODES];
    int i, head = 0, tail = 0, deepest = -1, maxrows = 1;
    for (i = 0; i < graph->node_count; ++i) { depth[i] = -1; rows[i] = links[i] = 0; }
    for (i = 0; i < graph->edge_count; ++i) {
        const struct graph_edge *e = &graph->edges[i];
        if (e->a == -1 && e->b >= 0 && e->b < graph->node_count && depth[e->b] < 0) {
            depth[e->b] = 0; queue[tail++] = e->b; deepest = 0;
        }
    }
    for (;;) {
        while (head < tail) {
            int node = queue[head++], j;
            for (j = 0; j < graph->edge_count; ++j) {
                const struct graph_edge *e = &graph->edges[j];
                int next = e->a == node ? e->b : e->b == node ? e->a : -1;
                if (next < 0 || next >= graph->node_count || depth[next] >= 0) continue;
                depth[next] = depth[node]+1;
                if (depth[next] > deepest) deepest = depth[next];
                queue[tail++] = next;
            }
        }
        for (i = 0; i < graph->node_count && depth[i] >= 0; ++i) {}
        if (i == graph->node_count) break;
        depth[i] = ++deepest; queue[tail++] = i;
    }
    /* Reserve a separate vertical track for every edge in each column gap.
     * Spacing grows with actual connectivity, not the number of processors. */
    for (i = 0; i < graph->edge_count; ++i) {
        const struct graph_edge *e = &graph->edges[i];
        if (e->a >= 0 && e->a < graph->node_count && e->b >= 0 && e->b < graph->node_count)
            ++links[depth[e->a] < depth[e->b] ? depth[e->a] : depth[e->b]];
    }
    columns[0] = 320;
    for (i = 1; i <= deepest; ++i) {
        int pitch = 256+16*links[i-1];
        columns[i] = columns[i-1]+(pitch < 320 ? 320 : pitch);
    }
    for (i = 0; i < graph->node_count; ++i) {
        int row = rows[depth[i]]++, x = columns[depth[i]], y = -210-row*260;
        struct graph_box *b = &graph->nodes[i].box;
        b->x0 = x-88; b->x1 = x+88; b->y0 = y-120; b->y1 = y+60;
        if (row+1 > maxrows) maxrows = row+1;
    }
    graph->width = columns[deepest]+256+16*links[deepest];
    if (graph->width < width) graph->width = width;
    graph->height = 440+(maxrows-1)*260;
}

void graph_sample(struct graph *graph)
{
    graph_empty(graph);
    graph->nodes = sample_nodes;
    graph->node_count = sizeof(sample_nodes) / sizeof(sample_nodes[0]);
    graph->edges = sample_edges;
    graph->edge_count = sizeof(sample_edges) / sizeof(sample_edges[0]);
    graph->sample = 1;
    graph->width = 1400;
    graph->height = 440;
    graph_select(graph, -1, 0);
}

static void compact_tracks(struct graph *graph, int width);

void graph_layout(struct graph *graph, int width)
{
    int centres[2], i;
    graph->height = 440; graph->host_y = -210; graph->layout_kind = GRAPH_EMPTY;
    if (!graph->node_count) {
        graph->width = width < 520 ? 520 : width;
        return;
    }
    if (graph->sample == 2) {
        graph->width = 960; graph->height = 960; graph->layout_kind = GRAPH_FIXED;
        for (i = 0; i < graph->node_count; ++i) {
            int x = 320+(i%3)*256, y = -210-(i/3)*260;
            graph->nodes[i].box.x0 = x-88;
            graph->nodes[i].box.x1 = x+88;
            graph->nodes[i].box.y0 = y-120;
            graph->nodes[i].box.y1 = y+60;
        }
        return;
    }
    if (graph->sample != 1 && graph->node_count != 2 && graph->node_count <= GRAPH_MAX_NODES) {
        if (graph->layout_mode == GRAPH_PREVIOUS) {
            baseline_layout(graph, width); graph->layout_kind = GRAPH_BASELINE;
        } else {
            quality_place(graph, width);
            if (graph->layout_kind == GRAPH_LAYERED) compact_tracks(graph, width);
        }
        return;
    }
    if (!graph->sample && graph->node_count != 2) {
        graph->width = 240+graph->node_count*256;
        for (i = 0; i < graph->node_count; ++i) {
            graph->nodes[i].box.x0 = 240+i*256;
            graph->nodes[i].box.x1 = 416+i*256;
            graph->nodes[i].box.y0 = -330;
            graph->nodes[i].box.y1 = -150;
        }
        return;
    }
    if (graph->node_count != 2) return;
    graph->layout_kind = GRAPH_COMPACT;
    if (width < 520) width = 520;
    graph->width = width;
    centres[0] = width/2;
    centres[1] = width-96;
    for (i = 0; i < 2; ++i) {
        graph->nodes[i].box.x0 = centres[i]-88;
        graph->nodes[i].box.x1 = centres[i]+88;
        graph->nodes[i].box.y0 = -330;
        graph->nodes[i].box.y1 = -150;
    }
}

int graph_node_y(const struct graph_node *node)
{
    return node->box.y1-60;
}

static int node_x(const struct graph *graph, int node)
{
    return node < 0 ? 96 : (graph->nodes[node].box.x0+graph->nodes[node].box.x1)/2;
}

/* Links within one column use its right-hand gap. All others attach on the
 * side facing the peer. Ports on a face have distinct, stable attachment points. */
static int face(const struct graph *graph, int node, int peer)
{
    return node_x(graph, peer) < node_x(graph, node) ? -1 : 1;
}

static void attachment(const struct graph *graph, int node, int peer, int port,
                       struct graph_point *point, struct graph_point *label)
{
    int side = face(graph, node, peer), count = 0, rank = 0, i, offset = 0;
    int cx = node_x(graph, node), cy = node < 0 ? graph->host_y : graph_node_y(&graph->nodes[node]);
    if (node >= 0) {
        for (i = 0; i < graph->edge_count; ++i) {
            const struct graph_edge *e = &graph->edges[i];
            int other, p;
            if (e->a == node) { other = e->b; p = e->aport; }
            else if (e->b == node) { other = e->a; p = e->bport; }
            else continue;
            if (face(graph, node, other) != side) continue;
            ++count;
            if (graph->layout_kind == GRAPH_BASELINE) {
                if (p < port) ++rank;
            } else {
                int peer_y = peer < 0 ? graph->host_y : graph_node_y(&graph->nodes[peer]);
                int other_y = other < 0 ? graph->host_y : graph_node_y(&graph->nodes[other]);
                int peer_number = peer < 0 ? -1 : graph->nodes[peer].number;
                int other_number = other < 0 ? -1 : graph->nodes[other].number;
                if (other_y > peer_y || (other_y == peer_y &&
                    (other_number < peer_number || (other_number == peer_number && p < port)))) ++rank;
            }
        }
        if (count == 2) offset = 24-48*rank;
        else if (count == 3) offset = 24-24*rank;
        else if (count >= 4) offset = 24-16*rank;
        /* Opposite faces use different scanlines, so links from neighbouring
         * processors cannot share a horizontal segment across a column gap. */
        if (side < 0) offset -= 4;
    }
    point->x = cx+side*34; point->y = cy+offset;
    label->x = cx+(side > 0 ? 44 : -76);
    if (count > 2 && (rank & 1)) label->x += side*40;
    label->y = point->y+4;
}

/* Colour disjoint vertical intervals onto reusable tracks. Horizontal socket
 * scanlines remain distinct; intervals include a small clearance at bends. */
static void compact_tracks(struct graph *graph, int width)
{
    int indices[GRAPH_MAX_EDGES], gap[GRAPH_MAX_EDGES];
    int high[GRAPH_MAX_EDGES], low[GRAPH_MAX_EDGES];
    int columns[GRAPH_MAX_NODES], lanes[GRAPH_MAX_NODES], nodecol[GRAPH_MAX_NODES];
    int n=0, nc=0, i,j,k;
    for(i=0;i<graph->node_count;++i) {
        int x=node_x(graph,i), at=0;
        while(at<nc && columns[at]<x)++at;
        if(at<nc && columns[at]==x)continue;
        for(j=nc;j>at;--j)columns[j]=columns[j-1];
        columns[at]=x;++nc;
    }
    for(i=0;i<graph->node_count;++i) {
        for(j=0;columns[j]!=node_x(graph,i);++j){}
        nodecol[i]=j;
    }
    memset(lanes,0,sizeof(lanes));
    for(i=0;i<graph->edge_count;++i) {
        const struct graph_edge *e=&graph->edges[i];
        struct graph_point a,b,label;
        int x;
        graph->tracks[i]=0;
        if(e->a<0)continue;
        x=node_x(graph,e->a);if(node_x(graph,e->b)<x)x=node_x(graph,e->b);
        for(j=0;columns[j]!=x;++j){}
        gap[i]=j;
        attachment(graph,e->a,e->b,e->aport,&a,&label);
        attachment(graph,e->b,e->a,e->bport,&b,&label);
        high[i]=a.y>b.y?a.y:b.y;low[i]=a.y<b.y?a.y:b.y;
        k=n;
        while(k>0) {
            int previous=indices[k-1];
            const struct graph_edge *f=&graph->edges[previous];
            int ea=graph->nodes[e->a].number*8+e->aport,eb=graph->nodes[e->b].number*8+e->bport;
            int fa=graph->nodes[f->a].number*8+f->aport,fb=graph->nodes[f->b].number*8+f->bport,t;
            if(ea>eb){t=ea;ea=eb;eb=t;}if(fa>fb){t=fa;fa=fb;fb=t;}
            if(gap[i]>gap[previous] || (gap[i]==gap[previous] &&
               (high[i]<high[previous] || (high[i]==high[previous] &&
               (ea>fa || (ea==fa && eb>=fb))))))break;
            indices[k]=previous;--k;
        }
        indices[k]=i;++n;
    }
    for(i=0;i<n;++i) {
        int edge=indices[i],lane=0,free_track;
        do {
            free_track=1;
            for(j=0;j<i;++j) {
                int previous=indices[j];
                if(gap[previous]==gap[edge] && graph->tracks[previous]==lane &&
                   low[previous]-8<=high[edge] && low[edge]-8<=high[previous]) {free_track=0;break;}
            }
            if(!free_track)++lane;
        }while(!free_track);
        graph->tracks[edge]=lane;
        if(lanes[gap[edge]]<lane+1)lanes[gap[edge]]=lane+1;
    }
    k=320;
    for(i=0;i<nc;++i) {
        int pitch=256+16*lanes[i];if(pitch<320)pitch=320;
        for(j=0;j<graph->node_count;++j)if(nodecol[j]==i) {
            graph->nodes[j].box.x0=k-88;graph->nodes[j].box.x1=k+88;
        }
        if(i==nc-1)graph->width=k+192+16*lanes[i];
        k+=pitch;
    }
    if(graph->width<width)graph->width=width;
}

static void layered_path(const struct graph *graph, int index, struct graph_path *path)
{
    const struct graph_edge *e = &graph->edges[index];
    int ax = node_x(graph, e->a), bx = node_x(graph, e->b);
    int column = ax < bx ? ax : bx, track = 0, i, x;
    attachment(graph, e->a, e->b, e->aport, &path->points[0], &path->label_a);
    attachment(graph, e->b, e->a, e->bport, &path->points[3], &path->label_b);
    if (e->a < 0) x = (ax+bx)/2;
    else {
        for (i = 0; i < index; ++i) {
            const struct graph_edge *previous = &graph->edges[i];
            int px, qx;
            if (previous->a < 0) continue;
            px = node_x(graph, previous->a); qx = node_x(graph, previous->b);
            if ((px < qx ? px : qx) == column) ++track;
        }
        if (graph->layout_kind == GRAPH_LAYERED) track = graph->tracks[index];
        x = column+128+16*track;
    }
    path->count = 4;
    path->points[1].x = path->points[2].x = x;
    path->points[1].y = path->points[0].y;
    path->points[2].y = path->points[3].y;
}

void graph_edge_path(const struct graph *graph, int index, struct graph_path *path)
{
    const struct graph_edge *e = &graph->edges[index];
    const struct graph_node *b = &graph->nodes[e->b];
    int bx = (b->box.x0+b->box.x1)/2, by = graph_node_y(b);
    int ax = 96, ay = graph->host_y, lane = 0, i, direction;
    if (graph->layout_kind == GRAPH_LAYERED || graph->layout_kind == GRAPH_BASELINE) {
        layered_path(graph, index, path);
        return;
    }
    if (e->a >= 0) {
        const struct graph_node *a = &graph->nodes[e->a];
        ax = (a->box.x0+a->box.x1)/2; ay = graph_node_y(a);
    }
    for (i = 0; i < index; ++i)
        if ((graph->edges[i].a == e->a && graph->edges[i].b == e->b) ||
            (graph->edges[i].a == e->b && graph->edges[i].b == e->a)) ++lane;
    if (ay == by) {
        int y = ay+(e->a < 0 ? 0 : 16-lane*32);
        direction = bx >= ax ? 1 : -1;
        path->count = 2;
        path->points[0].x = ax+direction*34; path->points[0].y = y;
        path->points[1].x = bx-direction*34; path->points[1].y = y;
        path->label_a.x = ax+(direction > 0 ? 44 : -76);
        path->label_b.x = bx+(direction > 0 ? -68 : 44);
        path->label_a.y = path->label_b.y = y+4;
    } else {
        int reverse = by > ay, temp;
        if (reverse) {
            temp = ax; ax = bx; bx = temp;
            temp = ay; ay = by; by = temp;
        }
        direction = -1;
        /* Captions are painted last: vertical links need no text detour. */
        path->count = 2;
        path->points[0].x = ax; path->points[0].y = ay-34;
        path->points[1].x = bx; path->points[1].y = by+34;
        if (ax != bx) {
            int middle=(ay+by)/2;
            path->count=4;
            path->points[3]=path->points[1];
            path->points[1].x=ax;path->points[1].y=middle;
            path->points[2].x=bx;path->points[2].y=middle;
        }
        path->label_a.x = ax+(direction < 0 ? 42 : 8);
        path->label_a.y = ay+(direction < 0 ? -40 : 40);
        path->label_b.x = bx+(direction < 0 ? 8 : 42);
        path->label_b.y = by+(direction < 0 ? 40 : -40);
        if (reverse) {
            struct graph_point point = path->label_a;
            path->label_a = path->label_b; path->label_b = point;
            for (i = 0; i < path->count/2; ++i) {
                point = path->points[i];
                path->points[i] = path->points[path->count-1-i];
                path->points[path->count-1-i] = point;
            }
        }
    }
}

void graph_select(struct graph *graph, int index, int adjust)
{
    int i;
    for (i = 0; i < graph->node_count; ++i) {
        if (!adjust) graph->nodes[i].selected = (i == index);
        else if (i == index) graph->nodes[i].selected = !graph->nodes[i].selected;
    }
}

int graph_hit(const struct graph *graph, int x, int y)
{
    int i;
    for (i = 0; i < graph->node_count; ++i) {
        const struct graph_box *b = &graph->nodes[i].box;
        if (x >= b->x0 && x < b->x1 && y >= b->y0 && y < b->y1)
            return i;
    }
    return -1;
}

int graph_pointer(const struct graph *graph, int x, int y,
                  int visible_x0, int visible_y1, int xscroll, int yscroll)
{
    return graph_hit(graph, x - visible_x0 + xscroll,
                     y - visible_y1 + yscroll);
}
