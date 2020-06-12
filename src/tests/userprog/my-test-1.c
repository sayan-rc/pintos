/* Try writing to fd 1 (stdout). */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  char *str = "write this to stdout\n";
  write (1, str, 21);
}
