# 在 dispatch_loops 的早期调用中追踪按名称查询对象自有字段的缓存路径。
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
printf "object_get_by_name_cached hit #%d\n", $hits
bt 6
if $hits >= 5
  quit
end
continue
end
run
