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
