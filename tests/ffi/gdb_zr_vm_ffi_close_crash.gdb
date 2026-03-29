set pagination off
set confirm off
handle SIGSEGV stop print nopass
run --verbose
bt
frame 0
info registers
quit
