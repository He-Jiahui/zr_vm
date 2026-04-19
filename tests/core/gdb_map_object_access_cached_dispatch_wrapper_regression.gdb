set pagination off
set confirm off
set print thread-events off
handle SIGSEGV stop print nopass
file ./build/benchmark-gcc-release/bin/zr_vm_cli
set args ./build/benchmark-gcc-release/tests_generated/performance_suite/cases/map_object_access/zr/benchmark_map_object_access.zrp
run
bt
frame 0
info locals
quit
