/* Yield before readiness checks, including a disconnected link. */
#include "hwlegacy.h"
#undef free
extern int TestRead(int);
extern int TestWrite(int);
extern int hardware_fault_read_blocked;
static int ready_read(int link)
{ hw_service(); return hardware_fault_read_blocked ? 0 : TestRead(link); }
static int ready_write(int link) { hw_service(); return TestWrite(link); }
#define TestRead ready_read
#define TestWrite ready_write
#include "driver.h"
