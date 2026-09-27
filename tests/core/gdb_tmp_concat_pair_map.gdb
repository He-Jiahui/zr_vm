# 用 map_object_access 工程采样 ConcatPair 的上游调用栈；文件内固定了旧 WSL gcc 构建路径。
# 需要对应二进制含该符号与调试信息，采样到第六次调用即退出调试器。
set pagination off
set breakpoint pending on
set print thread-events off
set $hits = 0
file ./build-wsl-gcc/bin/zr_vm_cli
set args ./tests/benchmarks/cases/map_object_access/zr/benchmark_map_object_access.zrp
break ZrCore_String_ConcatPair
commands
silent
set $hits = $hits + 1
printf "concat_pair hit #%d left=%p right=%p\n", $hits, left, right
bt 8
if $hits >= 6
  quit
end
continue
end
run
