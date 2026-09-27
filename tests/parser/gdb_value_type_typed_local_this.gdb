# 原意在 compiler_integration 单测编译 typed local 与 this 时观察当前类型及局部变量槽位。
# BUG: compiler_typed_metadata.c:1120/1132 当前处理导出函数参数类型，非 typed-local 绑定；第二断点没有 localVar。
set pagination off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_compiler_integration_test
break /mnt/e/Git/zr_vm/zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c:1120
commands
silent
printf "typed locals currentType=%p ", cs ? cs->currentTypeName : 0
if cs && cs->currentTypeName
  printf "%s", cs->currentTypeName->stringDataExtend
end
printf " count=%lu currentFunctionNode=%p\n", cs ? cs->localVars.length : 0, cs ? cs->currentFunctionNode : 0
continue
end
break /mnt/e/Git/zr_vm/zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c:1132
commands
silent
if localVar
  printf "  local slot=%u name=%p ", localVar->stackSlot, localVar->name
  if localVar->name
    printf "%s", localVar->name->stringDataExtend
  end
  printf "\n"
end
continue
end
run
