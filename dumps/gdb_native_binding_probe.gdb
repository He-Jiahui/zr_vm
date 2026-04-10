set breakpoint pending on
set env LD_LIBRARY_PATH=/mnt/e/Git/zr_vm/build/codex-wsl-current-gcc-debug-make/lib
file /mnt/e/Git/zr_vm/build/codex-wsl-current-gcc-debug-make/bin/zr_vm_cli
set args /mnt/e/Git/zr_vm/dumps/codex_tmp_set_to_map_project/set_to_map.zrp --execution-mode binary
break zr_container_set_get_iterator
commands
  silent
  printf "\n-- set_get_iterator --\n"
  print context
  print result
  bt 6
  continue
end
break zr_container_pair_equals
commands
  silent
  printf "\n-- pair_equals --\n"
  print context
  print *context
  print context->selfValue
  if (context->selfValue)
    print *context->selfValue
  end
  print ZrLib_CallContext_Argument(context, 0)
  if (ZrLib_CallContext_Argument(context, 0))
    print *ZrLib_CallContext_Argument(context, 0)
  end
  bt 6
  continue
end
run
