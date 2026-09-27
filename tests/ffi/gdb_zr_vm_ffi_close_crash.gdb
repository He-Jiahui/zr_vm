# 由 gdb --args <zr_vm_ffi_probe> 启动；保留崩溃时的栈和寄存器。
set pagination off
set confirm off
# TODO: --verbose 当前被 probe 当作未知 mode，落入 all-no-lib；核对这里是否应覆盖库句柄 close。
handle SIGSEGV stop print nopass
run --verbose
bt
frame 0
info registers
quit
