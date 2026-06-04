set pagination off
set breakpoint pending on
file ./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
break /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:4311
commands
silent
if currentFunction && currentFunction->instructionsLength == 19
    set $pc = programCounter - currentFunction->instructionsList
    if $pc >= 5 && $pc <= 16
        printf "pc=%ld op=%u extra=%u a1=%u b1=%u a2=%d | s2=%u/%p s3=%u/%p s4=%u/%p s5=%u/%p s6=%u/%p s8=%u/%p\n", $pc, instruction.instruction.operationCode, instruction.instruction.operandExtra, instruction.instruction.operand.operand1[0], instruction.instruction.operand.operand1[1], instruction.instruction.operand.operand2[0], base[2].value.type, base[2].value.value.object, base[3].value.type, base[3].value.value.object, base[4].value.type, base[4].value.value.object, base[5].value.type, base[5].value.value.object, base[6].value.type, base[6].value.value.object, base[8].value.type, base[8].value.value.object
    end
end
continue
end
break __assert_fail
run
