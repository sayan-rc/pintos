# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(my-test-2) begin
(process-b) run
process-b: exit(80)
(process-a) wait(exec()) = 80
process-a: exit(81)
(my-test-2) wait(exec()) = 81
(my-test-2) end
my-test-2: exit(0)
EOF
pass;
