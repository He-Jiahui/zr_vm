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
