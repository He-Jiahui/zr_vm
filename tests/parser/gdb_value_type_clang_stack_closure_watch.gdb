set pagination off
set breakpoint pending on
file ./build/codex-wsl-clang-debug/bin/zr_vm_value_type_runtime_test
break /mnt/e/Git/zr_vm/tests/parser/test_value_type_runtime.c:333
commands
    silent
    set $state = state
    set $stack_closure_slot = &state->stackClosureValueList
    printf "test state=%p stackClosureValueList=%p slot=%p exceptionSlot=%p\n", $state, $state->stackClosureValueList, $stack_closure_slot, &state->exceptionRecoverPoint
    watch -location *$stack_closure_slot
    commands
        silent
        printf "stackClosureValueList slot changed to %p at pc=%p\n", *$stack_closure_slot, $pc
        bt 12
        continue
    end
    continue
end
run
