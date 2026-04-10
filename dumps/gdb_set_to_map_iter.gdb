set pagination off
set breakpoint pending on
file /mnt/e/Git/zr_vm/build/codex-wsl-current-gcc-debug-make/bin/zr_vm_cli
set env LD_LIBRARY_PATH=/mnt/e/Git/zr_vm/build/codex-wsl-current-gcc-debug-make/lib
set args /mnt/e/Git/zr_vm/dumps/codex_tmp_set_to_map_project/set_to_map.zrp --execution-mode binary

break ZrCore_Object_IterMoveNext
commands
  silent
  printf "\n== IterMoveNext ==\n"
  printf "iterator type=%d object=%p result=%p\n", iteratorValue->type, iteratorValue->value.object, result
  if iteratorValue->value.object != 0
    printf "iterator internalType=%d prototype=%p memberVersion=%u\n", ((SZrObject*)iteratorValue->value.object)->internalType, ((SZrObject*)iteratorValue->value.object)->prototype, ((SZrObject*)iteratorValue->value.object)->memberVersion
  end
  bt 6
  continue
end

run
