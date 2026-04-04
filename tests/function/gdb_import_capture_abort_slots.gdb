set pagination off
set confirm off
set print pretty on

file ./build/codex-wsl-gcc-debug/bin/zr_vm_cli
set args ./tests/fixtures/projects/import_capture_native/import_capture_native.zrp

run

printf "Hit abort while running import_capture_native\n"
bt 12
frame 7

printf "\n===== current closure =====\n"
print closure
print closure->function
print closure->function->functionName
print closure->function->closureValueLength
if closure->function->closureValueLength > 0
  print closure->function->closureValueList[0]
  if closure->function->closureValueList[0].name->shortStringLength < 255
    printf "upvalue[0].name = %s\n", (char*)closure->function->closureValueList[0].name->stringDataExtend
  else
    printf "upvalue[0].name = %s\n", *(char**)closure->function->closureValueList[0].name->stringDataExtend
  end
  print closure->closureValuesExtend[0]
  print *(closure->closureValuesExtend[0]->value.valuePointer)
end
if closure->function->closureValueLength > 1
  print closure->function->closureValueList[1]
  if closure->function->closureValueList[1].name->shortStringLength < 255
    printf "upvalue[1].name = %s\n", (char*)closure->function->closureValueList[1].name->stringDataExtend
  else
    printf "upvalue[1].name = %s\n", *(char**)closure->function->closureValueList[1].name->stringDataExtend
  end
  print closure->closureValuesExtend[1]
  print *(closure->closureValuesExtend[1]->value.valuePointer)
end

printf "\n===== parent frame slots =====\n"
print callInfo->previous
if callInfo->previous != 0
  print callInfo->previous->functionBase.valuePointer
  print *(callInfo->previous->functionBase.valuePointer + 1)
  print *(callInfo->previous->functionBase.valuePointer + 2)
  print *(callInfo->previous->functionBase.valuePointer + 3)
  print *(callInfo->previous->functionBase.valuePointer + 4)
  print *(callInfo->previous->functionBase.valuePointer + 5)
  print *(callInfo->previous->functionBase.valuePointer + 6)
end

quit
