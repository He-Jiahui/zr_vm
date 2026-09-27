# 在 clang Debug 的 value_type_runtime 场景建立 state 闭包链表写监视，追踪捕获后的链表变更及调用栈。
# TODO: 监视点在该测试销毁 state 后仍保持启用；需核对后续测试是否复用同一地址，以免把失效地址的写入归到原场景。
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
