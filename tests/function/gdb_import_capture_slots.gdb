set pagination off
set confirm off
set print pretty on

file ./build/codex-wsl-gcc-debug/bin/zr_vm_cli
set args ./tests/fixtures/projects/import_capture_native/import_capture_native.zrp

break /mnt/d/Git/Github/zr_vm_mig/zr_vm/zr_vm_core/src/zr_vm_core/module/module.c:858
commands
silent
printf "\n===== module export collection =====\n"
print pathStr
print func->stackSize
print func->localVariableLength
if func->localVariableLength > 0
  print func->localVariableList[0]
end
if func->localVariableLength > 1
  print func->localVariableList[1]
end
if func->localVariableLength > 2
  print func->localVariableList[2]
end
if func->localVariableLength > 3
  print func->localVariableList[3]
end
print *(callBase + 1)
print *(callBase + 2)
print *(callBase + 3)
print *(callBase + 4)
print *(callBase + 5)
print *(callBase + 6)
continue
end

run
quit
