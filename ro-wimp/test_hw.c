#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "inmos.h"
#include "linkio.h"
#include "checklib.h"
#include "cklib.h"
#include "hardware.h"
extern void *hw_alloc(int);
extern void hw_service(void);
LINK TheLink;
struct tpstats *root;
char LinkName[64];
static int discoveries, loads, closes, replies, fail_reply, changed, cancel_now;
static int only_node;
static int progress_first, end_code = 2, oversized;
static int banked_result;
static int timing_mode;
static unsigned long requested[2];
static struct hardware a, b;
int OpenLink(char *name) { assert(!strcmp(name,"TMB")); return 3; }
int CloseLink(int link) { assert(link==3); ++closes; return 0; }
int bpw(int type) { (void)type; return 4; }
char *tptostr(int type) { return class(type) == T_805 ? "T805G" : "T800D"; }
struct tpstats *find(struct tpstats *p, int id)
{ for (;p;p=p->next) if(p->tpid==id) return p; return NULL; }
void check(int s,int c,int r,int info,int reset)
{
    struct tpstats *p,*q;
    assert(!s && !c && !r && !info && reset); ++discoveries;
    p=hw_alloc(sizeof(*p)); q=hw_alloc(sizeof(*q)); root=p;
    p->next=q; q->parent=p; q->routelen=1; q->route=1;
    p->tpid=0; q->tpid=1; p->tptype=T_805+6; q->tptype=changed?T_800:T_805+6;
    p->procspeed=q->procspeed=25;
    p->links[1]=q; p->linkno[1]=2; q->links[2]=p; q->linkno[2]=1;
    p->links[2]=q; p->linkno[2]=1; q->links[1]=p; q->linkno[1]=2;
    p->linkno[0]=HOST_TAG; replies=0;
    hw_service();
}
int tryroute(LINK link,struct tpstats *p,int last)
{ (void)link;(void)p;(void)last;hw_service();return 1; }
int load(LINK link,struct tpstats *p,long cs,long o,long ws,long vs,int bw,unsigned char *code)
{ (void)link;(void)p;(void)cs;(void)o;(void)ws;(void)vs;(void)bw;(void)code; ++loads;hw_service();return 1; }
int sendiserver(LINK link,unsigned int len,unsigned char *data)
{
    int id=data[0];(void)link; assert(len==8 && data[2]==132 && data[3]==4);
    requested[id]=(unsigned long)data[4] | ((unsigned long)data[5]<<8) |
      ((unsigned long)data[6]<<16) | ((unsigned long)data[7]<<24);
    return 1;
}
int getiserver(LINK link,unsigned int *len,unsigned char *data,unsigned int max)
{
    int id=only_node<0?(timing_mode ? replies/4 : progress_first ? replies/2 : replies):only_node;
    (void)link; assert(max==16); hw_service();
    if(fail_reply && replies==1) { *len=0;return 0; }
    memset(data,0,16);data[0]=id;data[2]=6;
    data[3]=(unsigned char)(progress_first && !(replies & 1) ? 0 : end_code);
    data[5]=254;data[6]=15; /* Last 2 KiB probe before the 4 MiB ceiling. */
    if (banked_result) { data[5]=2; data[6]=16; }
    if (timing_mode) {
        data[3] = (unsigned char)((replies%4)==3 ? 255 : 0);
        if ((replies%4)<2) {
            data[5] = (unsigned char)((replies%4)==0 ? 2 : 4); data[6]=0;
            data[8] = (unsigned char)((replies%4)==0 ? 42 : 126);
        }
    }
    if (oversized) data[7]=1;
    *len=16;++replies;return 1;
}
static int service(void) { return cancel_now; }
static int reenter(void)
{
    assert(!hardware_run(&b,0,-1,4096,NULL,NULL));
    return 0;
}
int main(void)
{
    int before;
    struct graph graph;
    graph_empty(&graph);
    only_node=-1;
    assert(hardware_run(&a,0,-1,4096,reenter,NULL));
    assert(a.count==2 && a.edge_count==3 && discoveries==1 && closes==1);
    memset(&graph, 0, sizeof(graph)); graph.width=592;
    hardware_graph(&a, &graph);
    assert(graph_hit(&graph, (graph.nodes[0].box.x0+graph.nodes[0].box.x1)/2, -210)==0);
    assert(graph_pointer(&graph, (graph.nodes[1].box.x0+graph.nodes[1].box.x1)/2+100-40,
                         -210+900+80, 100, 900, 40, -80)==1);
    assert(hardware_run(&b,1,-1,4096,service,NULL));
    assert(discoveries==2 && loads==2 && requested[0]==1048575UL && requested[1]==1048575UL);
    assert(b.nodes[0].ram_bytes==4194304UL && b.nodes[0].ram_state==2);
    assert(b.nodes[1].ram_state==2);
    progress_first=1;end_code=255;
    assert(hardware_run(&b,1,-1,4096,service,NULL));
    assert(b.nodes[0].ram_state==3 && b.nodes[0].ram_bytes==4194304UL);
    banked_result=1;
    assert(hardware_run(&b,1,-1,8192,service,NULL));
    assert(b.nodes[0].ram_state==3 && b.nodes[0].ram_bytes==4198400UL);
    assert(b.nodes[0].internal_bytes==4096UL && b.nodes[0].ram_bytes-b.nodes[0].internal_bytes==4194304UL);
    banked_result=0;
    timing_mode=1;banked_result=1;
    assert(hardware_run(&b,1,-1,8192,service,NULL));
    assert(b.nodes[0].internal_cycles==1 && b.nodes[0].external_cycles==3);
    assert(b.nodes[1].internal_cycles==1 && b.nodes[1].external_cycles==3);
    timing_mode=0;banked_result=0;
    end_code=1;
    assert(hardware_run(&b,1,-1,4096,service,NULL));
    assert(b.nodes[0].ram_state==4);
    end_code=254;
    assert(hardware_run(&b,1,-1,4096,service,NULL));
    assert(b.nodes[0].ram_state==5);
    progress_first=0;end_code=2;oversized=1;
    assert(!hardware_run(&b,1,-1,4096,service,NULL));
    assert(strstr(b.error,"exceeds"));oversized=0;
    fail_reply=1;
    assert(!hardware_run(&b,1,-1,4096,service,NULL));
    assert(b.ready && b.nodes[0].ram_state==2 && b.nodes[1].ram_state==1);
    fail_reply=0; changed=1;before=loads;
    assert(!hardware_run(&b,1,1,4096,service,&a));
    assert(strstr(b.error,"changed") && loads==before);
    changed=0;only_node=1;
    assert(hardware_run(&b,1,1,4096,service,&a));
    assert(loads==before+1 && !b.nodes[0].ram_state && b.nodes[1].ram_state==2);
    cancel_now=1;before=closes;
    assert(!hardware_run(&b,0,-1,4096,service,NULL));
    assert(strstr(b.error,"Cancelled") && closes==before);
    cancel_now=0;
    assert(hardware_run(&b,0,-1,4096,service,NULL));
    assert(!hardware_run(&b,1,-1,5,service,NULL));
    puts("Hardware ownership, fresh discovery, RAM limits, partial results and identity checks passed");
    return 0;
}
