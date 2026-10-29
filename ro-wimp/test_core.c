#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "graph.h"
#include "job.h"
#include "present.h"

static void ownership(void)
{
    struct job job;
    job_init(&job);
    assert(!job_can_quit(&job));
    assert(job_begin(&job));
    /* A nested event handler must not clear or replace the active owner. */
    assert(!job_begin(&job));
    assert(job.busy);
    job_cancel(&job);
    assert(job.cancel && job.busy);
    assert(!job_begin(&job));
    job_end(&job, 1);
    assert(!job.live && !job.busy);
    /* A new operation may follow completed cancellation, with fresh state. */
    assert(job_begin(&job));
    assert(!job.cancel);
    job_end(&job, 1);
    assert(job.live);
    assert(job_begin(&job));
    assert(!job.live);
    job_quit(&job);
    assert(job.cancel && !job_can_quit(&job));
    assert(!job_begin(&job));
    job_end(&job, 1);
    assert(job_can_quit(&job) && !job.live);
    assert(!job_begin(&job));
    job_init(&job);
    job_quit(&job);
    assert(job_can_quit(&job));
}

static void topology(void)
{
    struct graph graph;
    graph_sample(&graph);
    assert(graph.sample && graph.node_count == 2 && graph.edge_count == 3);
    assert(graph.edges[0].a == -1 && graph.edges[0].b == 0);
    assert(graph.edges[0].bport == 0);
    assert(graph.edges[1].a == graph.edges[2].a);
    assert(graph.edges[1].b == graph.edges[2].b);
    assert(graph.edges[1].aport != graph.edges[2].aport);
    assert(graph.edges[1].bport != graph.edges[2].bport);
    assert(graph_hit(&graph, 520, -210) == 0);
    assert(graph_hit(&graph, 960, -210) == 1);
    assert(graph_hit(&graph, 740, -210) == -1);
    assert(graph_hit(&graph, 400, -330) == 0);
    assert(graph_hit(&graph, 640, -330) == -1);
    assert(graph_hit(&graph, 400, -150) == -1);
    /* Moving and scrolling the window must not change the target node. */
    assert(graph_pointer(&graph, 520+100-40, -210+900+80,
                         100, 900, 40, -80) == 0);
    assert(graph_pointer(&graph, 960+300-120, -210+700+160,
                         300, 700, 120, -160) == 1);
    graph_select(&graph, 0, 0);
    assert(graph.nodes[0].selected && !graph.nodes[1].selected);
    graph_select(&graph, 1, 0);
    assert(!graph.nodes[0].selected && graph.nodes[1].selected);
    graph_select(&graph, 0, 1);
    assert(graph.nodes[0].selected && graph.nodes[1].selected);
    graph_select(&graph, 1, 1);
    assert(graph.nodes[0].selected && !graph.nodes[1].selected);
    graph_select(&graph, -1, 1); /* Adjust over blank space leaves selection. */
    assert(graph.nodes[0].selected);
    graph_select(&graph, -1, 0); /* Select over blank space clears selection. */
    assert(!graph.nodes[0].selected && !graph.nodes[1].selected);
    /* The complete sample fits the initial TV and VGA work areas. */
    graph_layout(&graph, 592);
    assert(graph.nodes[0].box.x0 > 130);
    assert(graph.nodes[0].box.x1 < graph.nodes[1].box.x0);
    assert(graph.nodes[1].box.x1 <= 592);
    assert(graph_hit(&graph, 496, -210) == 1);
    graph_layout(&graph, 752);
    assert(graph.nodes[1].box.x1 <= 752);
    assert(graph_hit(&graph, 656, -210) == 1);
}

static void grid_topology(void)
{
    struct graph graph;
    struct graph_path path;
    struct graph_path reversed;
    struct graph_edge edges[13];
    int ports[9][4], i, j;
    memset(ports, 0, sizeof(ports));
    graph_sample_grid(&graph);
    assert(graph.sample && graph.node_count == 9 && graph.edge_count == 13);
    assert(graph.width == 960 && graph.height == 960);
    for (i = 0; i < graph.node_count; ++i) {
        const struct graph_box *b = &graph.nodes[i].box;
        int x = (b->x0+b->x1)/2, y = graph_node_y(&graph.nodes[i]);
        assert(graph_hit(&graph, x, y) == i);
        assert(graph_pointer(&graph, x+100-200, y+900+520,
                             100, 900, 200, -520) == i);
        assert(b->x0 >= 0 && b->x1 <= graph.width);
        assert(y-144 >= -graph.height);
    }
    for (i = 0; i < graph.edge_count; ++i) {
        const struct graph_edge *e = &graph.edges[i];
        assert(e->b >= 0 && e->b < 9 && e->bport >= 0 && e->bport < 4);
        assert(!ports[e->b][e->bport]++);
        if (e->a >= 0) {
            assert(e->a < 9 && e->aport >= 0 && e->aport < 4);
            assert(!ports[e->a][e->aport]++);
        }
        graph_edge_path(&graph, i, &path);
        assert(path.count >= 2 && path.count <= 8);
        for (j = 0; j < path.count; ++j) {
            assert(path.points[j].x >= 0 && path.points[j].x < graph.width);
            assert(path.points[j].y < 0 && path.points[j].y > -graph.height);
            if (j) assert(path.points[j].x == path.points[j-1].x ||
                          path.points[j].y == path.points[j-1].y);
        }
        if (e->a >= 0 && e->b-e->a == 3) {
            assert(path.count == 2);
            assert(path.points[0].x == path.points[1].x);
        }
    }
    memcpy(edges, graph.edges, sizeof(edges));
    graph_edge_path(&graph, 2, &path);
    i = edges[2].a; edges[2].a = edges[2].b; edges[2].b = i;
    i = edges[2].aport; edges[2].aport = edges[2].bport; edges[2].bport = i;
    graph.edges = edges;
    graph_edge_path(&graph, 2, &reversed);
    assert(path.count == reversed.count);
    for (i = 0; i < path.count; ++i) {
        assert(path.points[i].x == reversed.points[path.count-1-i].x);
        assert(path.points[i].y == reversed.points[path.count-1-i].y);
    }
    assert(path.label_a.x == reversed.label_b.x && path.label_a.y == reversed.label_b.y);
    graph_sample(&graph);
    graph_layout(&graph, 592);
    graph_edge_path(&graph, 1, &path);
    i = path.points[0].y;
    graph_edge_path(&graph, 2, &path);
    assert(path.points[0].y != i); /* Preserve both parallel links. */
}

static void layout_bounds(const struct graph *graph)
{
    int i, j;
    struct graph_path path;
    for (i = 0; i < graph->node_count; ++i) {
        const struct graph_box *b = &graph->nodes[i].box;
        int x = (b->x0+b->x1)/2, y = graph_node_y(&graph->nodes[i]);
        assert(b->x0 >= 0 && b->x1 < graph->width && y-144 > -graph->height);
        assert(graph_pointer(graph, x+120-200, y+900+520,
                             120, 900, 200, -520) == i);
        for (j = 0; j < i; ++j) {
            const struct graph_box *a = &graph->nodes[j].box;
            assert(b->x0 >= a->x1 || a->x0 >= b->x1 ||
                   b->y0 >= a->y1 || a->y0 >= b->y1);
        }
    }
    for (i = 0; i < graph->edge_count; ++i) {
        graph_edge_path(graph, i, &path);
        for (j = 0; j < path.count; ++j) {
            assert(path.points[j].x >= 0 && path.points[j].x < graph->width);
            assert(path.points[j].y < 0 && path.points[j].y > -graph->height);
        }
    }
}

static int minimum(int a, int b) { return a < b ? a : b; }
static int maximum(int a, int b) { return a > b ? a : b; }

static void separate_routes(const struct graph *graph)
{
    struct graph_path paths[GRAPH_MAX_NODES*4];
    int i, j, a, b, n;
    assert(graph->edge_count <= GRAPH_MAX_NODES*4);
    for (i = 0; i < graph->edge_count; ++i) graph_edge_path(graph, i, &paths[i]);
    for (i = 0; i < graph->edge_count; ++i) {
        for (a = 1; a < paths[i].count; ++a) {
            struct graph_point p = paths[i].points[a-1], q = paths[i].points[a];
            assert(p.x == q.x || p.y == q.y);
            for (j = 0; j < i; ++j) {
                for (b = 1; b < paths[j].count; ++b) {
                    struct graph_point r = paths[j].points[b-1], t = paths[j].points[b];
                    /* A crossing may occur, but never a shared line segment. */
                    if (p.x == q.x && r.x == t.x && p.x == r.x)
                        assert(maximum(minimum(p.y,q.y), minimum(r.y,t.y)) >=
                               minimum(maximum(p.y,q.y), maximum(r.y,t.y)));
                    if (p.y == q.y && r.y == t.y && p.y == r.y)
                        assert(maximum(minimum(p.x,q.x), minimum(r.x,t.x)) >=
                               minimum(maximum(p.x,q.x), maximum(r.x,t.x)));
                }
            }
            for (n = 0; n < graph->node_count; ++n) {
                const struct graph_node *node = &graph->nodes[n];
                int cx = (node->box.x0+node->box.x1)/2, cy = graph_node_y(node);
                int left=cx-34, right=cx+34, bottom=cy-34, top=cy+34;
                /* Filled captions paint over wires; sprites remain obstacles. */
                if (p.x == q.x && p.x > left && p.x < right)
                    assert(maximum(p.y,q.y) <= bottom || minimum(p.y,q.y) >= top);
                if (p.y == q.y && p.y > bottom && p.y < top)
                    assert(maximum(p.x,q.x) <= left || minimum(p.x,q.x) >= right);
            }
        }
    }
    /* Distinct sockets, including both edges that used to join at node 6. */
    for (i = 0; i < graph->edge_count; ++i) {
        for (j = 0; j < i; ++j) {
            assert(paths[i].points[0].x != paths[j].points[0].x ||
                   paths[i].points[0].y != paths[j].points[0].y);
            assert(paths[i].points[0].x != paths[j].points[paths[j].count-1].x ||
                   paths[i].points[0].y != paths[j].points[paths[j].count-1].y);
            assert(paths[i].points[paths[i].count-1].x != paths[j].points[0].x ||
                   paths[i].points[paths[i].count-1].y != paths[j].points[0].y);
            assert(paths[i].points[paths[i].count-1].x != paths[j].points[paths[j].count-1].x ||
                   paths[i].points[paths[i].count-1].y != paths[j].points[paths[j].count-1].y);
        }
    }
}

static void automatic_topology(void)
{
    struct graph graph;
    struct graph_node nodes[GRAPH_MAX_NODES];
    struct graph_edge edges[GRAPH_MAX_NODES];
    struct graph_box saved[9];
    int i, x, y;
    graph_sample_layered(&graph);
    assert(graph.sample == 3 && graph.node_count == 9 && graph.edge_count == 13);
    /* The two routes to the opposite corner must share a shortest-hop layer. */
    for (i = 0; i < 9; ++i) {
        assert(graph.nodes[i].box.x0 == 232+320*(i/3+i%3));
        saved[i] = graph.nodes[i].box;
    }
    assert(graph.height >= 440 && graph.width <= 1856);
    layout_bounds(&graph);
    separate_routes(&graph);
    {
        struct graph_edge flipped[13];
        struct graph_path before[13], after;
        int j, temp;
        memcpy(flipped, graph.edges, sizeof(flipped));
        for (i = 0; i < 13; ++i) graph_edge_path(&graph, i, &before[i]);
        for (i = 1; i < 13; ++i) {
            temp = flipped[i].a; flipped[i].a = flipped[i].b; flipped[i].b = temp;
            temp = flipped[i].aport; flipped[i].aport = flipped[i].bport; flipped[i].bport = temp;
        }
        graph.edges = flipped;
        graph_layout(&graph, 752);
        separate_routes(&graph);
        for (i = 1; i < 13; ++i) {
            graph_edge_path(&graph, i, &after);
            assert(before[i].count == after.count);
            for (j = 0; j < after.count; ++j) {
                assert(before[i].points[j].x == after.points[after.count-1-j].x);
                assert(before[i].points[j].y == after.points[after.count-1-j].y);
            }
            assert(before[i].label_a.x == after.label_b.x && before[i].label_a.y == after.label_b.y);
            assert(before[i].label_b.x == after.label_a.x && before[i].label_b.y == after.label_a.y);
        }
    }
    graph_sample_layered(&graph);
    graph.sample = 0; /* The synthetic fixture uses the live layout unchanged. */
    graph_layout(&graph, 592);
    for (i = 0; i < 9; ++i) assert(!memcmp(&saved[i], &graph.nodes[i].box, sizeof(saved[i])));
    /* Host entry is not necessarily index zero. Include a cycle, a reversed
     * parallel connection, a cross-link within one layer and an isolated node. */
    memset(nodes, 0, sizeof(nodes));
    memset(edges, 0, sizeof(edges));
    graph.nodes = nodes; graph.node_count = 6; graph.edges = edges; graph.edge_count = 7;
    edges[0].a = -1; edges[0].b = 3;
    edges[1].a = 3; edges[1].b = 1;
    edges[2].a = 1; edges[2].b = 3;
    edges[3].a = 3; edges[3].b = 4;
    edges[4].a = 4; edges[4].b = 1;
    edges[5].a = 1; edges[5].b = 0;
    edges[6].a = 0; edges[6].b = 4;
    edges[1].aport = 1;
    edges[2].aport = 1; edges[2].bport = 2;
    edges[3].aport = 3;
    edges[4].aport = 1; edges[4].bport = 2;
    edges[5].aport = 3;
    edges[6].aport = 1; edges[6].bport = 2;
    graph_layout(&graph, 752);
    separate_routes(&graph);
    assert(nodes[3].box.x0 < nodes[1].box.x0);
    assert(nodes[1].box.x0 == nodes[4].box.x0);
    assert(nodes[0].box.x0 > nodes[1].box.x0);
    assert(nodes[2].box.x0 > nodes[0].box.x0 && nodes[5].box.x0 > nodes[2].box.x0);
    layout_bounds(&graph);
    /* Maximum supported network: no queue overflow, clipped extents or lost
     * nodes, and a second layout preserves both selection and hit targets. */
    graph.node_count = GRAPH_MAX_NODES; graph.edge_count = GRAPH_MAX_NODES;
    for (i = 0; i < GRAPH_MAX_NODES; ++i) {
        edges[i].a = i-1; edges[i].b = i;
        edges[i].aport = 1; edges[i].bport = 0;
    }
    graph_layout(&graph, 752); layout_bounds(&graph); separate_routes(&graph);
    graph_select(&graph, GRAPH_MAX_NODES-1, 0);
    graph_layout(&graph, 592);
    x = nodes[GRAPH_MAX_NODES-1].box.x0+88;
    y = graph_node_y(&nodes[GRAPH_MAX_NODES-1]);
    assert(nodes[GRAPH_MAX_NODES-1].selected && graph_hit(&graph, x, y) == GRAPH_MAX_NODES-1);
    /* A dense four-port torus exercises crowded column gaps and sockets. */
    graph.node_count = 16; graph.edge_count = 1;
    edges[0].a = -1; edges[0].b = 0; edges[0].bport = 3;
    for (i = 0; i < 16; ++i) {
        int e;
        if (i != 3) {
            e = graph.edge_count++;
            edges[e].a = i; edges[e].aport = 1;
            edges[e].b = i/4*4+(i+1)%4; edges[e].bport = 3;
        }
        e = graph.edge_count++;
        edges[e].a = i; edges[e].aport = 2;
        edges[e].b = (i+4)%16; edges[e].bport = 0;
    }
    graph_layout(&graph, 752); layout_bounds(&graph); separate_routes(&graph);
    /* Single-node maps still reserve room for the host connection. */
    graph.node_count = 1; graph.edge_count = 1;
    graph_layout(&graph, 752); layout_bounds(&graph); separate_routes(&graph);
}

static void example_topologies(void)
{
    struct graph g;
    int example,mode,i,j;
    for(example=0;example<GRAPH_EXAMPLES;++example) {
        struct graph_box saved[GRAPH_MAX_NODES];
        struct graph_node shuffled[GRAPH_MAX_NODES];
        struct graph_edge edges[GRAPH_MAX_EDGES];
        int ports[GRAPH_MAX_NODES][4];
        graph_empty(&g);assert(graph_example(&g,example));
        memset(ports,0,sizeof(ports));
        for(i=0;i<g.edge_count;++i) {
            const struct graph_edge *e=&g.edges[i];
            assert(e->b>=0 && e->b<g.node_count && e->bport>=0 && e->bport<4);
            assert(!ports[e->b][e->bport]++);
            if(e->a>=0) {assert(e->a<g.node_count && e->aport>=0 && e->aport<4);assert(!ports[e->a][e->aport]++);}
        }
        for(mode=0;mode<3;++mode) {
            g.layout_mode=mode;graph_layout(&g,752);
            layout_bounds(&g);separate_routes(&g);
            if(mode==GRAPH_AUTO) {
                if(example==0)assert(g.layout_kind==GRAPH_PIPELINE);
                if(example==1)assert(g.layout_kind==GRAPH_RING);
                if(example==2 || example==3)assert(g.layout_kind==GRAPH_GRID);
            }
            for(i=0;i<g.node_count;++i)saved[i]=g.nodes[i].box;
            g.nodes[0].ram_state=2;g.nodes[0].ram_bytes=8192UL*1024;
            graph_layout(&g,592);
            for(i=0;i<g.node_count;++i)assert(!memcmp(&saved[i],&g.nodes[i].box,sizeof(saved[i])) || g.node_count==2);
        }
        /* Renumbering storage and reversing record order must not shuffle the
         * picture. Processor numbers and port identities remain unchanged. */
        if(g.node_count==2)continue;
        g.layout_mode=GRAPH_AUTO;graph_layout(&g,752);
        for(i=0;i<g.node_count;++i){saved[i]=g.nodes[i].box;shuffled[g.node_count-1-i]=g.nodes[i];}
        for(i=0;i<g.edge_count;++i) {
            edges[g.edge_count-1-i]=g.edges[i];
            if(edges[g.edge_count-1-i].a>=0)edges[g.edge_count-1-i].a=g.node_count-1-g.edges[i].a;
            edges[g.edge_count-1-i].b=g.node_count-1-g.edges[i].b;
        }
        g.nodes=shuffled;g.edges=edges;graph_layout(&g,752);
        layout_bounds(&g);separate_routes(&g);
        for(i=0;i<g.node_count;++i) {
            j=g.node_count-1-i;
            assert(!memcmp(&saved[i],&g.nodes[j].box,sizeof(saved[i])));
        }
    }
    graph_empty(&g);assert(!graph_example(&g,-1));assert(!graph_example(&g,GRAPH_EXAMPLES));
}

static void varied_networks(void)
{
    struct graph g;
    struct graph_node nodes[GRAPH_MAX_NODES];
    struct graph_edge edges[GRAPH_MAX_EDGES];
    int used[GRAPH_MAX_NODES],seed,i,n,a,b,attempts,mode;
    unsigned long random=1;
    for(seed=0;seed<64;++seed) {
        graph_empty(&g);memset(nodes,0,sizeof(nodes));memset(used,0,sizeof(used));
        n=8+seed%9;g.nodes=nodes;g.node_count=n;g.edges=edges;g.edge_count=1;
        edges[0].a=-1;edges[0].aport=-1;edges[0].b=0;edges[0].bport=used[0]++;
        for(i=0;i<n;++i)nodes[i].number=i;
        for(i=1;i<n;++i) {
            struct graph_edge *e=&edges[g.edge_count++];
            e->a=i-1;e->aport=used[i-1]++;e->b=i;e->bport=used[i]++;
        }
        for(attempts=0;attempts<40;++attempts) {
            struct graph_edge *e;
            random=(random*1664525UL+1013904223UL)&0xffffffffUL;a=(int)((random>>8)%n);
            random=(random*1664525UL+1013904223UL)&0xffffffffUL;b=(int)((random>>8)%n);
            if(a==b || used[a]>=4 || used[b]>=4)continue;
            e=&edges[g.edge_count++];e->a=a;e->aport=used[a]++;e->b=b;e->bport=used[b]++;
        }
        for(mode=0;mode<2;++mode) {
            g.layout_mode=mode;graph_layout(&g,752);layout_bounds(&g);separate_routes(&g);
        }
    }
}

static void empty_topology(void)
{
    struct graph graph;
    graph_empty(&graph);
    graph_layout(&graph, 752);
    assert(!graph.sample && !graph.node_count && !graph.edge_count);
    assert(graph.width == 752 && graph.height == 440);
    assert(graph_hit(&graph, 520, -210) == -1);
    graph_select(&graph, 0, 1);
    graph_sample_grid(&graph);
    graph_select(&graph, 8, 0);
    graph_empty(&graph);
    assert(!graph.nodes && !graph.edges && !graph.sample);
    assert(!graph.node_count && !graph.edge_count);
    assert(graph_hit(&graph, 832, -730) == -1);
    graph_sample(&graph);
    assert(graph.node_count == 2 && !graph.nodes[0].selected);
}

int main(void)
{
    struct graph_node node;
    char value[80];
    ownership();
    topology();
    grid_topology();
    automatic_topology();
    empty_topology();
    example_topologies();
    varied_networks();
    memset(&node, 0, sizeof(node));
    node.internal_bytes = 4096;
    node.ram_bytes = 4198400UL; node.ram_state = 3;
    assert(node_external_bytes(&node) == 4194304UL);
    node_ram_label(&node, value);
    assert(!strcmp(value, "4/4096 KiB"));
    node_ram_value(&node, 0, value);
    assert(!strcmp(value, "4 KiB, 1 cycle"));
    node_ram_value(&node, 1, value);
    assert(!strcmp(value, "4096 KiB, cycles unknown"));
    node.external_cycles = 3;
    node_ram_value(&node, 1, value);
    assert(!strcmp(value, "4096 KiB, 3 cycles"));
    node.ram_state = 2; node.ram_bytes = 4194304UL;
    node_ram_label(&node, value);
    assert(!strcmp(value, "4/>=4092K"));
    node_ram_value(&node, 1, value);
    assert(!strcmp(value, ">=4092 KiB (ceiling)"));
    node.ram_bytes = 262144UL*1024;
    node_ram_label(&node, value);
    assert(!strcmp(value, "4/>=262140K") && strlen(value) <= 11);
    node.internal_bytes = 0;
    node.ram_state = 3; node.ram_bytes = 4194304UL;
    node_ram_label(&node, value);
    assert(!strcmp(value, "4096 KiB"));
    node.ram_state = 1; node.ram_bytes = 0;
    node_ram_value(&node, 1, value); assert(!strcmp(value, "Incomplete"));
    node.ram_state = 5;
    node_ram_value(&node, 1, value); assert(!strcmp(value, "Unknown (alias detected)"));
    node.ram_state = 3; node.ram_errors = 1;
    node_ram_value(&node, 1, value); assert(!strcmp(value, "Memory errors detected"));
    puts("WIMP ownership, cancellation, topology, selection and scrolled hit testing passed");
    return 0;
}
