# 从已加载的 CLI 观察 map_object_access 的短字符串驻留热路径和上游调用栈。
# 按 length 限定文本输出；断点指向 string.c 的 static helper，需可见调试符号。
set pagination off
set breakpoint pending on
set $hits = 0
break string_create_short
commands
silent
set $hits = $hits + 1
printf "string_create_short hit #%d len=%llu text=%.*s\n", $hits, (unsigned long long)length, (int)length, string
bt 3
if $hits >= 20
  quit
end
continue
end
run /mnt/e/Git/zr_vm/tests/benchmarks/cases/map_object_access/zr/benchmark_map_object_access.zrp
