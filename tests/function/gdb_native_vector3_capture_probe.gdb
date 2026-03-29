set pagination off
set confirm off
set print pretty on

file ./build/codex-wsl-gcc-debug/bin/zr_vm_cli
set args ./tests/projects/native_vector3_capture_probe/native_vector3_capture_probe.zrp

break execution.c:1956
commands
silent
set $key = (SZrString*) opB->value.object
if $key != 0 && $key->shortStringLength < 255 && strcmp((char*) $key->stringDataExtend, "vector3") == 0
  printf "\n===== GETTABLE math.vector3 =====\n"
  print callInfo->function
  print closure
  print *opA
  if opA->type == ZR_VALUE_TYPE_OBJECT
    set $module = (SZrObjectModule*) opA->value.object
    print $module
    print $module->moduleName
    print $module->fullPath
    print $module->super.nodeMap.elementCount
    print ZrCore_Object_GetValue(state, (SZrObject*) opA->value.object, opB)
  end
end
continue
end

break execution.c:1701
commands
silent
if opA != 0 && ZR_VALUE_IS_TYPE_NULL(opA->type)
  printf "\n===== NULL FUNCTION_CALL =====\n"
  print functionSlot
  print parametersCount
  print expectedReturnCount
  print instruction
  print *opA
  print callInfo->function
  bt 12
end
continue
end

run
quit
