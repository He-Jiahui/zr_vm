set pagination off
set confirm off
set print pretty on
set breakpoint pending on

file /mnt/e/Git/zr_vm/build/codex-wsl-current-gcc-debug-make/bin/zr_vm_cli
set args /mnt/e/Git/zr_vm/dumps/codex_tmp_set_to_map_project/set_to_map.zrp --execution-mode binary
set environment LD_LIBRARY_PATH /mnt/e/Git/zr_vm/build/codex-wsl-current-gcc-debug-make/lib

set $pair_ctor_hit = 0

break /mnt/e/Git/zr_vm/zr_vm_lib_container/src/zr_vm_lib_container/module.c:814
commands
  silent
  set $pair_ctor_hit = $pair_ctor_hit + 1
  printf "\n[pair_ctor hit %d]\n", $pair_ctor_hit
  set $arg0 = ZrLib_CallContext_Argument(context, 0)
  set $arg1 = ZrLib_CallContext_Argument(context, 1)
  if $arg0 != 0
    printf "arg0 type=%d int=%lld obj=%p\n", $arg0->type, (long long)$arg0->value.nativeObject.nativeInt64, $arg0->value.object
  else
    printf "arg0=<null>\n"
  end
  if $arg1 != 0
    printf "arg1 type=%d obj=%p\n", $arg1->type, $arg1->value.object
  else
    printf "arg1=<null>\n"
  end
  printf "pair=%p\n", pair
  if pair != 0
    set $first_value = zr_container_get_field_value(context->state, pair, "first")
    set $second_value = zr_container_get_field_value(context->state, pair, "second")
    if $first_value != 0
      printf "pair.first type=%d int=%lld obj=%p\n", $first_value->type, (long long)$first_value->value.nativeObject.nativeInt64, $first_value->value.object
    else
      printf "pair.first=<null>\n"
    end
    if $second_value != 0
      printf "pair.second type=%d obj=%p\n", $second_value->type, $second_value->value.object
    else
      printf "pair.second=<null>\n"
    end
  end
  continue
end

run
