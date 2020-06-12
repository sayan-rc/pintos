# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_USER_FAULTS => 1, [<<'EOF']);
(my-test-1) begin
write this to stdout
(my-test-1) end
my-test-1: exit(0)
EOF
pass;
