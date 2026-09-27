# 原意是在 dispatch_loops 的千次预热后采样成员读取缓存入口与 PIC 计数。
# BUG: 当前 execution_member_get_cached 仅转发至 _impl，函数帧没有 entry 局部变量；
# print entry 及其后的计数查询无法在该断点作用域得到有效数据。
set pagination off
set breakpoint pending on
set print thread-events off
set $hits = 0
file ./build-wsl-gcc/bin/zr_vm_cli
set args ./tests/benchmarks/cases/dispatch_loops/zr/benchmark_dispatch_loops.zrp
break execution_member_get_cached
commands
silent
set $hits = $hits + 1
if $hits < 1000
  continue
end
printf "member_get_cached hit #%d cacheIndex=%u receiver=%p result=%p\n", $hits, cacheIndex, receiver, result
print entry
if entry != 0
  print entry->picSlotCount
  print entry->runtimeHitCount
  print entry->runtimeMissCount
end
bt 4
if $hits >= 1003
  quit
end
continue
end
run
