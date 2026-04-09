set pagination off
set confirm off
set print pretty on
set breakpoint pending on

file /mnt/e/Git/zr_vm/build/codex-wsl-current-gcc-debug-make/bin/zr_vm_cli
set args /mnt/e/Git/zr_vm/dumps/codex_tmp_set_to_map_project/set_to_map.zrp --execution-mode binary
set environment LD_LIBRARY_PATH /mnt/e/Git/zr_vm/build/codex-wsl-current-gcc-debug-make/lib

set $array_add_hit = 0

break zr_container_array_add
commands
  silent
  set $array_add_hit = $array_add_hit + 1
  printf "\n[array_add hit %d]\n", $array_add_hit
  set $arg = ZrLib_CallContext_Argument(context, 0)
  if $arg == 0
    printf "arg=<null>\n"
  else
    printf "arg type=%d gc=%d obj=%p\n", $arg->type, $arg->isGarbageCollectable, $arg->value.object
    if $arg->value.object != 0
      set $pair_object = (SZrObject *)$arg->value.object
      set $first_value = zr_container_get_field_value(context->state, $pair_object, "first")
      set $second_value = zr_container_get_field_value(context->state, $pair_object, "second")
      if $first_value != 0
        printf "first type=%d int=%lld obj=%p\n", $first_value->type, (long long)$first_value->value.nativeObject.nativeInt64, $first_value->value.object
      else
        printf "first=<null>\n"
      end
      if $second_value != 0
        printf "second type=%d obj=%p\n", $second_value->type, $second_value->value.object
      else
        printf "second=<null>\n"
      end
    end
  end
  continue
end

run
