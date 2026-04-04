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
