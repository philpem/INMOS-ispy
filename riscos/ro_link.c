/* --
   -- Multi-tasking link support for Desktop IServer
   --
   -- RO_Link.c
   --
   -- Richard Crook
   -- February 1992
*/

#include "oslib/os.h"

extern int TestRead (int);
extern int TestWrite(int);
extern int _ReadLink (int, char*, unsigned int, int);
extern int _WriteLink(int, char*, unsigned int, int);


/* Read monotonic time ---------------------------------------------------------------------------------- */

static unsigned int MonotonicTime(void)
{
  int t;
  (void)xos_read_monotonic_time(&t);
  return t;
}

/* ReadLink() ------------------------------------------------------------------------------------------- */

int ReadLink(int LinkId, char* Buffer, unsigned int Count, int Timeout)
{
  int          i, n = 0;
  unsigned int time = MonotonicTime() + Timeout*10;

  while (Count != 0)
  {
    if (TestRead(LinkId))
    {
      i = _ReadLink(LinkId, Buffer, Count, Timeout);
      Buffer += i;
      Count  -= i;
      n      += i;
    }
    if (Timeout != 0 && (MonotonicTime() - time) < 0x80000000) break;
  }

  return n;
}

/* WriteLink() ------------------------------------------------------------------------------------------ */

int WriteLink(int LinkId, char* Buffer, unsigned int Count, int Timeout)
{
  int          i, n = 0;
  unsigned int time = MonotonicTime() + Timeout*10;

  while (Count != 0)
  {
    if (TestWrite(LinkId))
    {
      i = _WriteLink(LinkId, Buffer, Count, Timeout);
      Buffer += i;
      Count  -= i;
      n      += i;
    }
    if (Timeout != 0 && (MonotonicTime() - time) < 0x80000000) break;
  }

  return n;
}

/* End of file ========================================================================================== */
