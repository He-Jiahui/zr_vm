# 与早期样本配对，跳过前 199 次按名称读取以观察基准预热后的调用栈。
# 第 200 至 205 次共六次采样后退出。
# TODO: 目标是 static ZR_FORCE_INLINE；核查旧 WSL gcc CLI 的 GDB 能否在该 helper 下断点。
set pagination off
set breakpoint pending on
set print thread-events off
set $hits = 0
file ./build-wsl-gcc/bin/zr_vm_cli
set args ./tests/benchmarks/cases/dispatch_loops/zr/benchmark_dispatch_loops.zrp
break object_get_own_string_value_by_name_cached_unchecked
commands
silent
set $hits = $hits + 1
if $hits < 200
  continue
end
printf "object_get_by_name_cached hit #%d\n", $hits
bt 6
if $hits >= 205
  quit
end
continue
end
run
