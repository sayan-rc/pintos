#include <stdio.h>
#include "tests/lib.h"

const char *test_name = "process-a";

int
main (void)
{
  msg ("wait(exec()) = %d", wait (exec("process-b")));
  return 81;
}
