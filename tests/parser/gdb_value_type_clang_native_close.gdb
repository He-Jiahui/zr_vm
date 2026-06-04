set pagination off
set breakpoint pending on
file ./build/codex-wsl-clang-debug/bin/zr_vm_value_type_runtime_test
break zr_vm_core/src/zr_vm_core/closure.c:385
break ZrCore_Closure_FindOrCreateValue
commands 2
    silent
    printf "FindOrCreateValue stackPointer=%p base=%p top=%p tail=%p\n", stackPointer, state->stackBase.valuePointer, state->stackTop.valuePointer, state->stackTail.valuePointer
    bt 6
    continue
end
commands 1
    silent
    printf "close threshold=%p stackTop=%p stackBase=%p stackTail=%p\n", stackPointer, state->stackTop.valuePointer, state->stackBase.valuePointer, state->stackTail.valuePointer
    printf "state-list=%p local-closureValue=%p\n", state->stackClosureValueList, closureValue
    set $cv = state->stackClosureValueList
    printf "list captured=%p closed-link-next=%p closed-link-prev=%p\n", $cv->value.valuePointer, $cv->link.next, $cv->link.previous
    printf "list captured-base-delta=%ld captured-threshold-delta=%ld captured-top-delta=%ld\n", $cv->value.valuePointer - state->stackBase.valuePointer, $cv->value.valuePointer - stackPointer, state->stackTop.valuePointer - $cv->value.valuePointer
    x/8gx $cv
    set $ci = state->callInfoList
    set $depth = 0
    while $ci != 0 && $depth < 6
        printf "ci[%d]=%p base=%p top=%p previous=%p status=%u returnDest=%p hasReturn=%u\n", $depth, $ci, $ci->functionBase.valuePointer, $ci->functionTop.valuePointer, $ci->previous, $ci->callStatus, $ci->returnDestination, $ci->hasReturnDestination
        set $depth = $depth + 1
        set $ci = $ci->previous
    end
    bt 12
    continue
end
run
