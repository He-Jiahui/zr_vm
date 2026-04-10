set breakpoint pending on
set env LD_LIBRARY_PATH=/mnt/e/Git/zr_vm/build/codex-wsl-current-gcc-debug-make/lib
file /mnt/e/Git/zr_vm/build/codex-wsl-current-gcc-debug-make/bin/zr_vm_cli
set args /mnt/e/Git/zr_vm/dumps/codex_tmp_set_to_map_project/set_to_map.zrp --execution-mode binary
break object_call.c:375
commands
  silent
  printf "\n-- object_call after return before restore --\n"
  print savedCallInfo
  if (savedCallInfo)
    print savedCallInfo->functionBase.valuePointer
    print savedCallInfo->functionTop.valuePointer
  end
  print state->callInfoList
  if (state->callInfoList)
    print state->callInfoList->functionBase.valuePointer
    print state->callInfoList->functionTop.valuePointer
  end
  continue
end
break object_call.c:383
commands
  silent
  printf "\n-- object_call after restore --\n"
  print savedCallInfo
  if (savedCallInfo)
    print savedCallInfo->functionBase.valuePointer
    print savedCallInfo->functionTop.valuePointer
  end
  print state->callInfoList
  if (state->callInfoList)
    print state->callInfoList->functionBase.valuePointer
    print state->callInfoList->functionTop.valuePointer
  end
  continue
end
run
