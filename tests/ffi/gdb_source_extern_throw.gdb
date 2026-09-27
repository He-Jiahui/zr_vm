# 跟踪 source extern 异常抛出时的恢复点和线程状态。
set pagination off
set print pretty on
set breakpoint pending on
set confirm off

file ./build/codex-wsl-gcc-debug/bin/zr_vm_ffi_test

break test_zr_ffi_source_extern_can_bind_and_call_symbol
run

# TODO: 使用前按当前异常抛出入口重定位旧绝对路径与行号。
break /mnt/d/Git/Github/zr_vm_mig/zr_vm/zr_vm_core/src/zr_vm_core/exception.c:461
continue

printf "threadStatus=%d recover=%p\n", state->threadStatus, state->exceptionRecoverPoint
if state->exceptionRecoverPoint
  printf "recover->previous=%p recover->status=%d\n", state->exceptionRecoverPoint->previous, state->exceptionRecoverPoint->status
end
bt 12
