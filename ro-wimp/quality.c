/* Bounded, deterministic layout for at most 64 processors. No host libraries. */
#include "graph.h"
#include <string.h>

static unsigned char adjacent[GRAPH_MAX_NODES][GRAPH_MAX_NODES];
static unsigned char distance_to[GRAPH_MAX_NODES][GRAPH_MAX_NODES];
static int degree[GRAPH_MAX_NODES], depth[GRAPH_MAX_NODES];
static int best[GRAPH_MAX_NODES], pos[GRAPH_MAX_NODES];
static int count[GRAPH_MAX_NODES], queue[GRAPH_MAX_NODES], yy[GRAPH_MAX_NODES];

static int before(const struct graph *g, int a, int b)
{
    return g->nodes[a].number < g->nodes[b].number;
}

static void box(struct graph *g, int i, int x, int y)
{
    g->nodes[i].box.x0 = x-88; g->nodes[i].box.x1 = x+88;
    g->nodes[i].box.y0 = y-120; g->nodes[i].box.y1 = y+60;
}

static void distances(int source, int n)
{
    int head = 0, tail = 0, i;
    memset(distance_to[source], 255, sizeof(distance_to[source]));
    distance_to[source][source] = 0; queue[tail++] = source;
    while (head < tail) {
        int a = queue[head++];
        for (i = 0; i < n; ++i) if (adjacent[a][i] && distance_to[source][i] == 255) {
            distance_to[source][i] = (unsigned char)(distance_to[source][a]+1);
            queue[tail++] = i;
        }
    }
}

static int pipeline(struct graph *g, int root)
{
    int i, a = root, previous = -1;
    if (degree[root] != 1) return 0;
    for (i = 0; i < g->node_count; ++i) if (degree[i] < 1 || degree[i] > 2) return 0;
    for (i = 0; i < g->node_count; ++i) {
        int j, next = -1;
        box(g, a, 320+i*256, -210);
        for (j = 0; j < g->node_count; ++j) if (adjacent[a][j] && j != previous) next = j;
        previous = a; a = next;
        if (a < 0) break;
    }
    if (i != g->node_count-1) return 0;
    g->width = 320+(g->node_count-1)*256+144; g->height = 440;
    g->layout_kind = GRAPH_PIPELINE; return 1;
}

static int mesh(struct graph *g, int root)
{
    int n = g->node_count, w, b, i, j, ci, candidates[GRAPH_MAX_NODES];
    int xx[GRAPH_MAX_NODES], row[GRAPH_MAX_NODES], used[GRAPH_MAX_NODES];
    if (degree[root] != 2 || n < 4) return 0;
    for(i=0;i<n;++i) {
        j=i;
        while(j>0 && before(g,i,candidates[j-1])){candidates[j]=candidates[j-1];--j;}
        candidates[j]=i;
    }
    distances(root, n);
    for (w = 2; w <= n/2; ++w) if (n%w == 0 && n/w >= 2) {
        int h = n/w;
        for (ci = 0; ci < n; ++ci) {
            int good = 1;
            b=candidates[ci];
            if(degree[b]!=2 || distance_to[root][b]!=w-1)continue;
            distances(b, n); memset(used, 0, sizeof(used));
            for (i = 0; i < n; ++i) {
                int twice = distance_to[root][i]-distance_to[b][i]+w-1;
                if (twice & 1) { good = 0; break; }
                xx[i] = twice/2; row[i] = distance_to[root][i]-xx[i];
                if (xx[i] < 0 || xx[i] >= w || row[i] < 0 || row[i] >= h ||
                    used[row[i]*w+xx[i]]++) { good = 0; break; }
            }
            if (!good) continue;
            for (i = 0; i < n && good; ++i) for (j = 0; j < i; ++j) {
                int dx = xx[i]-xx[j], dy = row[i]-row[j];
                if (dx < 0) dx = -dx;
                if (dy < 0) dy = -dy;
                if ((dx+dy == 1) != (adjacent[i][j] != 0)) { good = 0; break; }
            }
            if (good) {
                for (i = 0; i < n; ++i) box(g, i, 320+xx[i]*256, -210-row[i]*260);
                g->width = 320+(w-1)*256+144; g->height = 440+(h-1)*260;
                g->layout_kind = GRAPH_GRID; return 1;
            }
        }
    }
    return 0;
}

static int ring(struct graph *g, int root)
{
    int i, a = root, previous = -1, n = g->node_count, top = (n+1)/2, bottom = n-top;
    if (n < 4) return 0;
    for (i = 0; i < n; ++i) if (degree[i] != 2) return 0;
    for (i = 0; i < n; ++i) {
        int j, next = -1, x = i < top ? i : top-1-(i-top)*(top-1)/(bottom-1);
        box(g, a, 320+x*256, i < top ? -210 : -470);
        for (j = 0; j < n; ++j) if (adjacent[a][j] && j != previous &&
            (next < 0 || before(g,j,next))) next = j;
        previous = a; a = next;
        if (a < 0 || (a == root && i != n-1)) return 0;
    }
    if (a != root) return 0;
    g->width = 320+(top-1)*256+144; g->height = 700;
    g->layout_kind = GRAPH_RING; return 1;
}

static int crossing_count(const struct graph *g)
{
    int i,j,total = 0;
    for (i = 0; i < g->edge_count; ++i) {
        const struct graph_edge *e = &g->edges[i];
        int a = e->a, b = e->b, t;
        if (a < 0 || depth[a] == depth[b]) continue;
        if (depth[a] > depth[b]) { t=a; a=b; b=t; }
        for (j=0;j<i;++j) {
            const struct graph_edge *f = &g->edges[j];
            int c=f->a,d=f->b;
            if(c<0 || depth[c]==depth[d]) continue;
            if(depth[c]>depth[d]) {t=c;c=d;d=t;}
            if(depth[a]==depth[c] && (pos[a]-pos[c])*(pos[b]-pos[d])<0) ++total;
        }
    }
    return total;
}

static void layered(struct graph *g, int root)
{
    int n=g->node_count, i,j,k,level,maxdepth=0,pass,bestscore;
    int key[GRAPH_MAX_NODES], members[GRAPH_MAX_NODES];
    memset(count,0,sizeof(count));
    distances(root,n);
    for(i=0;i<n;++i) {
        depth[i]=distance_to[root][i]==255 ? -1 : distance_to[root][i];
        if(depth[i]>maxdepth) maxdepth=depth[i];
    }
    /* Keep disconnected components separate; never infer a missing link. */
    for(i=0;i<n;++i) if(depth[i]<0) {
        int base=++maxdepth;
        distances(i,n);
        for(j=0;j<n;++j) if(depth[j]<0 && distance_to[i][j]!=255) {
            depth[j]=base+distance_to[i][j];
            if(depth[j]>maxdepth) maxdepth=depth[j];
        }
    }
    for(i=0;i<n;++i) ++count[depth[i]];
    for(level=0;level<=maxdepth;++level) {
        k=0;
        for(i=0;i<n;++i) if(depth[i]==level) {
            j=k;
            while(j>0 && before(g,i,members[j-1])) { members[j]=members[j-1]; --j; }
            members[j]=i; ++k;
        }
        for(j=0;j<k;++j) pos[members[j]]=j;
    }
    bestscore=crossing_count(g); memcpy(best,pos,sizeof(best));
    /* Six alternating neighbour-order sweeps, retaining the best crossing count. */
    for(pass=0;pass<6;++pass) {
        int direction=(pass&1)?-1:1;
        for(k=0;k<=maxdepth;++k) {
            level=direction>0?k:maxdepth-k;
            j=0;
            for(i=0;i<n;++i) if(depth[i]==level) {
                int v,sum=0,links=0;
                members[j++]=i;
                for(v=0;v<n;++v) if(adjacent[i][v] && depth[v]==level-direction) {
                    sum+=pos[v]*1024-(count[depth[v]]-1)*512; ++links;
                }
                key[i]=links ? sum/links : pos[i]*1024-(count[level]-1)*512;
            }
            for(i=1;i<j;++i) {
                int a=members[i],p=i;
                while(p>0 && (key[a]<key[members[p-1]] ||
                      (key[a]==key[members[p-1]] && pos[a]<pos[members[p-1]]))) {
                    members[p]=members[p-1]; --p;
                }
                members[p]=a;
            }
            for(i=0;i<j;++i) pos[members[i]]=i;
        }
        i=crossing_count(g);
        if(i<bestscore) {bestscore=i;memcpy(best,pos,sizeof(best));}
    }
    memcpy(pos,best,sizeof(pos));
    for(i=0;i<n;++i) yy[i]=pos[i]*260-(count[depth[i]]-1)*130;
    /* Align to neighbours, packing each column without changing its order. */
    for(pass=0;pass<4;++pass) for(k=1;k<=maxdepth;++k) {
        int sumwanted=0,sumactual=0;
        level=(pass&1)?maxdepth-k+1:k;
        for(i=0;i<n;++i) if(depth[i]==level) {
            int v,sum=0,links=0;
            members[pos[i]]=i;
            for(v=0;v<n;++v) if(adjacent[i][v]) {sum+=yy[v];++links;}
            key[i]=links?sum/links:yy[i]; sumwanted+=key[i];
        }
        for(i=0;i<count[level];++i) {
            int a=members[i]; yy[a]=key[a];
            if(i && yy[a]<yy[members[i-1]]+260) yy[a]=yy[members[i-1]]+260;
            sumactual+=yy[a];
        }
        for(i=0;i<count[level];++i) yy[members[i]]+=(sumwanted-sumactual)/count[level];
    }
    /* Every processor centre uses the same eight-unit lattice. Socket offsets
     * then give opposite faces disjoint scanlines even after centring columns.
     * Rounding can reduce the 260-unit row gap to 256, still above clearance. */
    for(i=0;i<n;++i) yy[i]=yy[i]>=0 ? ((yy[i]+4)/8)*8 : -((-yy[i]+4)/8)*8;
    j=yy[0];k=yy[0];
    for(i=1;i<n;++i) {if(yy[i]<j)j=yy[i];if(yy[i]>k)k=yy[i];}
    for(i=0;i<n;++i) box(g,i,320+depth[i]*320,-210-(yy[i]-j));
    g->host_y=graph_node_y(&g->nodes[root]);
    g->width=320+maxdepth*320+192;g->height=440+k-j;g->layout_kind=GRAPH_LAYERED;
}

void quality_place(struct graph *g, int width)
{
    int i, root=0, simple=1, hosts=0;
    memset(adjacent,0,sizeof(adjacent));memset(degree,0,sizeof(degree));
    for(i=0;i<g->edge_count;++i) {
        const struct graph_edge *e=&g->edges[i];
        if(e->a<0) {root=e->b;++hosts;continue;}
        if(e->a==e->b || adjacent[e->a][e->b]) simple=0;
        else {adjacent[e->a][e->b]=adjacent[e->b][e->a]=1;++degree[e->a];++degree[e->b];}
    }
    g->host_y=-210;
    if(g->layout_mode==GRAPH_AUTO && simple && hosts==1 &&
       (pipeline(g,root) || mesh(g,root) || ring(g,root))) {
        if(g->width<width)g->width=width;
        return;
    }
    layered(g,root);
    if(g->width<width)g->width=width;
}

const char *graph_layout_name(const struct graph *g)
{
    static const char *names[]={"Empty","Compact","Pipeline","Ring","Grid","Layered","Host distance","Fixed grid"};
    return names[g->layout_kind>=0 && g->layout_kind<8 ? g->layout_kind : 0];
}
