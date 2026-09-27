# 手工复查 map_object_access 基准运行时的崩溃栈；需先生成下方 tests_generated 工程并保留对应 Release 符号。
# 正常退出后没有故障帧可供 bt/locals 查询；此脚本不参与 CTest 自动断言。
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
