# 以 Release 单测二进制复查缓存派发包装层的中止或段错误现场；先构建 CMake 的对应目标。
# bt/locals 仅在信号使进程停住时有故障帧，优化构建也可能隐藏局部变量。
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
