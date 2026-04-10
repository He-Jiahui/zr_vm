set breakpoint pending on
set env LD_LIBRARY_PATH=/mnt/e/Git/zr_vm/build/codex-wsl-current-gcc-debug-make/lib
file /mnt/e/Git/zr_vm/build/codex-wsl-current-gcc-debug-make/bin/zr_vm_cli
set args /mnt/e/Git/zr_vm/dumps/codex_tmp_set_to_map_project/set_to_map.zrp --execution-mode binary
break ZrCore_Object_CallValue if argumentCount==0
commands
  silent
  printf "\n-- callvalue enter --\n"
  print state->callInfoList
  if (state->callInfoList)
    print state->callInfoList->functionBase.valuePointer
    print state->callInfoList->functionTop.valuePointer
  end
  finish
  printf "\n-- callvalue exit --\n"
  print $rax
  print state->callInfoList
  if (state->callInfoList)
    print state->callInfoList->functionBase.valuePointer
    print state->callInfoList->functionTop.valuePointer
  end
  continue
end
run
