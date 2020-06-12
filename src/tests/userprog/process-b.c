#include <stdio.h>
#include "tests/lib.h"

const char *test_name = "process-b";

int
main (void)
{
  msg("run");
  return 80;
}
