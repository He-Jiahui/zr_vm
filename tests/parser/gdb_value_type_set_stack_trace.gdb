set pagination off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
break /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:3070
commands 1
silent
if currentFunction && currentFunction->instructionsLength == 19
    set $pc = programCounter - currentFunction->instructionsList
    if $pc == 8 || $pc == 9 || $pc == 13
        printf "SET pc=%ld dst=%u src=%u before phys2=%u/%p phys3=%u/%p phys4=%u/%p phys5=%u/%p srcLog=%u/%p dstLog=%u/%p\n", $pc, instruction.instruction.operandExtra, instruction.instruction.operand.operand2[0], base[2].value.type, base[2].value.value.object, base[3].value.type, base[3].value.value.object, base[4].value.type, base[4].value.value.object, base[5].value.type, base[5].value.value.object, execution_inline_frame_get_value_slot(state,currentFunction,base,instruction.instruction.operand.operand2[0])->type, execution_inline_frame_get_value_slot(state,currentFunction,base,instruction.instruction.operand.operand2[0])->value.object, execution_inline_frame_get_value_slot(state,currentFunction,base,instruction.instruction.operandExtra)->type, execution_inline_frame_get_value_slot(state,currentFunction,base,instruction.instruction.operandExtra)->value.object
    end
end
continue
end
break __assert_fail
run
