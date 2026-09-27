# 在 dispatch_loops 基准中追踪成员描述符缓存读取的上游调用栈；依赖旧 WSL gcc CLI 路径。
# 该断点只说明函数被调用，不区分缓存命中或回退；第八次命中即停止。
set pagination off
set breakpoint pending on
set print thread-events off
set $hits = 0
file ./build-wsl-gcc/bin/zr_vm_cli
set args ./tests/benchmarks/cases/dispatch_loops/zr/benchmark_dispatch_loops.zrp
break ZrCore_Object_GetMemberCachedDescriptorUnchecked
commands
silent
set $hits = $hits + 1
printf "cached_descriptor_get hit #%d\n", $hits
bt 6
if $hits >= 8
  quit
end
continue
end
run
