# 从已加载的 CLI 运行 map_object_access，筛选以 a/b/c/d 开头的两字节文本及以 _ 开头的五字节文本。
# TODO: ZrCore_String_Create 接受显式 length；x/s 仍按 NUL 结尾读取，复用到非终止缓冲区前核查来源。
set pagination off
set breakpoint pending on
set $hits = 0
break ZrCore_String_Create
commands
silent
if (((length == 2) && ((string[0] == 'a') || (string[0] == 'b') || (string[0] == 'c') || (string[0] == 'd'))) || ((length == 5) && (string[0] == '_')))
  set $hits = $hits + 1
  printf "bench-literal create hit #%d len=%llu text=", $hits, (unsigned long long)length
  x/s string
  bt 4
  if $hits >= 20
    quit
  end
end
continue
end
run /mnt/e/Git/zr_vm/tests/benchmarks/cases/map_object_access/zr/benchmark_map_object_access.zrp
