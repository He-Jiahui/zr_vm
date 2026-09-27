# 编译期导入别名诊断脚本；仅在命中 Serializable/markFunction 时打印调用栈。
# TODO: file/args/break 均指向旧工作区绝对路径；本仓库的 compiler_locals.c 已移至 compiler/，需按现行路径复验。
set pagination off
set breakpoint pending on
file /mnt/d/Git/Github/zr_vm_mig/zr_vm/build/codex-wsl-clang-debug/bin/zr_vm_cli
set args --compile /mnt/d/Git/Github/zr_vm_mig/zr_vm/tests/fixtures/projects/decorator_compile_time_import/decorator_compile_time_import.zrp --intermediate
break /mnt/d/Git/Github/zr_vm_mig/zr_vm/zr_vm_parser/src/zr_vm_parser/compiler_locals.c:7
commands
silent
set $name = ZrCore_String_GetNativeString(name)
if $name
  if strcmp($name, "Serializable") == 0 || strcmp($name, "markFunction") == 0
    bt
  end
end
continue
end
run
quit
