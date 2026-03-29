set pagination off
set confirm off
set print pretty on

file ./build/codex-wsl-gcc-debug/bin/zr_vm_cli
set args ./tests/projects/import_capture_native/import_capture_native.zrp

break ZrCore_Closure_PushToStack
commands
silent
printf "\n===== ZrCore_Closure_PushToStack =====\n"
print function
print function->functionName
if function->functionName != 0
  if function->functionName->shortStringLength < 255
    printf "function name = %s\n", (char*)function->functionName->stringDataExtend
  else
    printf "function name = %s\n", *(char**)function->functionName->stringDataExtend
  end
end
print function->closureValueLength
if function->closureValueLength > 0
  print function->closureValueList[0]
end
if function->closureValueLength > 1
  print function->closureValueList[1]
end
print base
print *base
print *(base + 1)
print *(base + 2)
print *(base + 3)
print *(base + 4)
print *(base + 5)
continue
end

run
quit
