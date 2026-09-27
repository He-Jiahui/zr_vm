# 原意是在 map_object_access 的只读映射热路径之后才采样临时 GC 根创建，避免无关调用噪声。
# TODO: 首断点指向 zr_vm_lib_container 的 static ZR_FORCE_INLINE 回调；核查旧 Release 构建的
# GDB 是否能解析内联断点，否则预先禁用的第二断点始终不会被启用。
set pagination off
set breakpoint pending on
set print thread-events off
set $temp_root_hits = 0
file ./build/benchmark-gcc-release/bin/zr_vm_cli
set args ./tests/benchmarks/cases/map_object_access/zr/benchmark_map_object_access.zrp
break zr_container_map_get_item_readonly_inline_fast
commands
silent
printf "entered map readonly-inline hot path, enabling temp-root breakpoint\n"
enable 2
continue
end
break ZrLib_TempValueRoot_Begin
disable 2
commands 2
silent
set $temp_root_hits = $temp_root_hits + 1
printf "temp_root_begin hit #%d\n", $temp_root_hits
bt 6
if $temp_root_hits >= 5
    quit
end
continue
end
run
