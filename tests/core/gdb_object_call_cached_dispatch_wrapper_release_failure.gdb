set pagination off
set confirm off
set print thread-events off
handle SIGABRT stop print nopass
handle SIGSEGV stop print nopass
file ./build/benchmark-gcc-release/bin/zr_vm_object_call_known_native_fast_path_test
run
bt
frame 0
info locals
quit
