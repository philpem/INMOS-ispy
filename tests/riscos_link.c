#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../riscos/ro_link.c"
static int mode, raw_calls;
static unsigned int ticks;
void *xos_read_monotonic_time(int *t)
{
    *t = (int)ticks++;
    return mode == 5 ? (void *)"clock error" : NULL;
}
int TestRead(int link) { return mode == 1 ? 0 : mode == 4 ? -1 : 1; }
int TestWrite(int link) { return TestRead(link); }
int _ReadLink(int link, char *buf, unsigned int count, int timeout)
{
    raw_calls++;
    if (count > 256) abort();
    memset(buf, 'x', count);
    return mode == 2 ? -1 : mode == 3 ? (int)count+1 : (int)count;
}
int _WriteLink(int link, char *buf, unsigned int count, int timeout)
{ return _ReadLink(link, buf, count, timeout); }
int main(int argc, char **argv)
{
    char buf[600];
    int n;
    mode = atoi(argv[1]);
    ticks = argc > 2 ? (unsigned int)-5 : 0;
    n = ReadLink(0, buf, mode == 6 ? sizeof(buf) : 8, 1);
    printf("%d %d\n", n, raw_calls);
    return 0;
}
