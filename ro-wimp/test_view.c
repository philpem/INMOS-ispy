/* Host-only preview driver: dump the exact production placement and paths. */
#include <stdio.h>
#include <stdlib.h>
#include "graph.h"
#include "present.h"
int main(int argc,char **argv)
{
    struct graph g;int i,j;char ram[80];
    if(argc!=3)return 1;
    graph_empty(&g);g.width=752;g.layout_mode=atoi(argv[2]);
    if(!graph_example(&g,atoi(argv[1])))return 1;
    printf("G %d %d %d %s\n",g.width,g.height,g.host_y,graph_layout_name(&g));
    for(i=0;i<g.node_count;++i){node_ram_label(&g.nodes[i],ram);
        printf("N %d %d %d %s %s\n",g.nodes[i].number,(g.nodes[i].box.x0+g.nodes[i].box.x1)/2,
            graph_node_y(&g.nodes[i]),g.nodes[i].type,ram);}
    for(i=0;i<g.edge_count;++i){struct graph_path p;const struct graph_edge *e=&g.edges[i];
        graph_edge_path(&g,i,&p);
        printf("E %d %d %d %d %d %d %d %d %d",e->a,e->aport,e->b,e->bport,p.count,p.label_a.x,p.label_a.y,p.label_b.x,p.label_b.y);
        for(j=0;j<p.count;++j)printf(" %d %d",p.points[j].x,p.points[j].y);
        puts("");
    }
    return 0;
}
