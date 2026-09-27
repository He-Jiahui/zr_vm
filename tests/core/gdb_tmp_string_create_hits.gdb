# 从已加载的 CLI 运行 map_object_access，观察包括键名在内的字符串创建调用栈。
# TODO: 此接口按 length 接收字节，x/s 按 NUL 终止读取；若输入改为切片，先核查原始缓冲区边界。
set pagination off
set breakpoint pending on
set $hits = 0
break ZrCore_String_Create
commands
silent
set $hits = $hits + 1
printf "ZrCore_String_Create hit #%d len=%llu text=", $hits, (unsigned long long)length
x/s string
bt 3
if $hits >= 30
  quit
end
continue
end
run /mnt/e/Git/zr_vm/tests/benchmarks/cases/map_object_access/zr/benchmark_map_object_access.zrp
