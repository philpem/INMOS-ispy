/* Synthetic textbook-style networks; link numbers are illustrative, not wiring instructions. */
#include "graph.h"
#include <string.h>
static struct graph_node nodes[GRAPH_MAX_NODES];
static struct graph_edge edges[GRAPH_MAX_EDGES];
static int ports[GRAPH_MAX_NODES], edge_count;
static const char *names[]={"Pipeline","Ring","Grid 3x3","Grid 4x4","Binary tree","Cube","Irregular","Parallel links"};

const char *graph_example_name(int example)
{
    return example>=0 && example<GRAPH_EXAMPLES ? names[example] : "Unknown";
}

static void link_nodes(int a,int b)
{
    struct graph_edge *e=&edges[edge_count++];
    e->a=a;e->aport=a<0?-1:ports[a]++;e->b=b;e->bport=ports[b]++;
}

int graph_example(struct graph *g,int example)
{
    int i,n=8,w=0,width=g->width,mode=g->layout_mode;
    if(example<0 || example>=GRAPH_EXAMPLES)return 0;
    graph_empty(g);g->width=width;g->layout_mode=mode;
    memset(nodes,0,sizeof(nodes));memset(ports,0,sizeof(ports));edge_count=0;
    if(example==2){w=3;n=9;}else if(example==3){w=4;n=16;}
    else if(example==4)n=7;else if(example==7)n=2;
    for(i=0;i<n;++i){nodes[i].number=i;nodes[i].type="T805G";nodes[i].mhz=25;
        nodes[i].internal_bytes=4096;nodes[i].ram_bytes=4198400UL;nodes[i].ram_state=3;
        nodes[i].internal_cycles=1;nodes[i].external_cycles=3;}
    link_nodes(-1,0);
    if(example==0 || example==1){for(i=0;i<n-1;++i)link_nodes(i,i+1);if(example==1)link_nodes(n-1,0);}
    else if(w){for(i=0;i<n;++i){if(i%w<w-1)link_nodes(i,i+1);if(i/w<w-1)link_nodes(i,i+w);}}
    else if(example==4){for(i=1;i<n;++i)link_nodes((i-1)/2,i);}
    else if(example==5){int bit;for(i=0;i<n;++i)for(bit=1;bit<=4;bit*=2)if(i<(i^bit))link_nodes(i,i^bit);}
    else if(example==6){static const int pairs[][2]={{0,1},{0,2},{1,3},{1,4},{2,4},{2,5},{3,6},{4,6},{5,7},{6,7}};
        for(i=0;i<10;++i)link_nodes(pairs[i][0],pairs[i][1]);}
    else {link_nodes(0,1);link_nodes(0,1);}
    g->nodes=nodes;g->node_count=n;g->edges=edges;g->edge_count=edge_count;g->sample=4;
    graph_layout(g,width);return 1;
}
