# 原意是在 GC 传播迭代上限处输出队列和迭代计数。
# BUG: gc_mark.c:906 现为 gray-list 去重断言，当前作用域没有 iterationCount/maxIterations；
# 断点命中后不能产生预期上限证据，应重定位传播循环的条件点。
set pagination off
set confirm off
file /mnt/e/Git/zr_vm/build-wsl-gcc/bin/zr_vm_cli
set args /mnt/e/Git/zr_vm/build/benchmark-gcc-release/tests_generated/performance_suite/cases/gc_fragment_stress/zr/bench_array_plus_map/bench_array_plus_map.zrp
break gc_mark.c:906
commands
  silent
  printf "\n=== propagate limit hit ===\n"
  bt 6
  p iterationCount
  p maxIterations
  p global->garbageCollector->waitToScanObjectList
  p global->garbageCollector->waitToScanAgainObjectList
  quit 2
end
run
