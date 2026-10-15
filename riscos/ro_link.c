/* --
   -- Multi-tasking link support for Desktop IServer
   --
   -- RO_Link.c
   --
   -- Richard Crook
   -- February 1992
*/

#include "oslib/os.h"
#include <limits.h>

extern int TestRead (int);
extern int TestWrite(int);
extern int _ReadLink (int, char*, unsigned int, int);
extern int _WriteLink(int, char*, unsigned int, int);


/* Read monotonic time ---------------------------------------------------------------------------------- */

static int MonotonicTime(unsigned int *ticks)
{
  int t;
  if (xos_read_monotonic_time(&t) != 0) return -1;
  *ticks = (unsigned int)t;
  return 0;
}

/* ReadLink() ------------------------------------------------------------------------------------------- */

int ReadLink(int LinkId, char* Buffer, unsigned int Count, int Timeout)
{
  int          i, n = 0;
  unsigned int time, now, chunk;
  if (Count == 0) return 0;
  if (Timeout < 0 || Count > INT_MAX || MonotonicTime(&now) < 0) return -1;
  time = now + (unsigned int)Timeout*10;

  while (Count != 0)
  {
    if ((i = TestRead(LinkId)) < 0) return -1;
    if (i)
    {
      /* The assembly polls only a bounded number of times per byte, but
       * ignores Timeout. Limit each burst, then check the wall deadline. */
      chunk = Count > 256 ? 256 : Count;
      i = _ReadLink(LinkId, Buffer, chunk, Timeout);
      if (i < 0 || (unsigned int)i > chunk) return -1;
      Buffer += i;
      Count  -= i;
      n      += i;
    }
    if (MonotonicTime(&now) < 0) return -1;
    if (Timeout != 0 && (now - time) < 0x80000000U) break;
  }

  return n;
}

/* WriteLink() ------------------------------------------------------------------------------------------ */

int WriteLink(int LinkId, char* Buffer, unsigned int Count, int Timeout)
{
  int          i, n = 0;
  unsigned int time, now, chunk;
  if (Count == 0) return 0;
  if (Timeout < 0 || Count > INT_MAX || MonotonicTime(&now) < 0) return -1;
  time = now + (unsigned int)Timeout*10;

  while (Count != 0)
  {
    if ((i = TestWrite(LinkId)) < 0) return -1;
    if (i)
    {
      chunk = Count > 256 ? 256 : Count;
      i = _WriteLink(LinkId, Buffer, chunk, Timeout);
      if (i < 0 || (unsigned int)i > chunk) return -1;
      Buffer += i;
      Count  -= i;
      n      += i;
    }
    if (MonotonicTime(&now) < 0) return -1;
    if (Timeout != 0 && (now - time) < 0x80000000U) break;
  }

  return n;
}

/* End of file ========================================================================================== */
