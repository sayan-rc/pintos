/* Try waiting for a subprocess (process a) that executes another process
   (process b) before finishing. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  msg ("wait(exec()) = %d", wait (exec ("process-a")));
}
