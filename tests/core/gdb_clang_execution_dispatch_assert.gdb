set pagination off
set confirm off
set print pretty on
set breakpoint pending on

# Use with:
# gdb -q -x ./tests/core/gdb_clang_execution_dispatch_assert.gdb --args \
#   ./build/codex-wsl-current-clang-debug-make/bin/zr_vm_cli \
#   ./tests/fixtures/projects/hello_world/hello_world.zrp

break /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/execution_dispatch.c:1766
commands
  silent
  printf "\n=== execution_dispatch BITWISE_XOR assert ===\n"
  printf "programCounter=%p instructionsEnd=%p offset=%lld\n", \
         (void *)programCounter, (void *)instructionsEnd, (long long)(programCounter - closure->function->instructionsList)
  printf "opcode=%u raw=0x%016llx E=%u A1=%u B1=%u\n", \
         (unsigned)instruction.instruction.operationCode, \
         (unsigned long long)instruction.value, \
         (unsigned)instruction.instruction.operandExtra, \
         (unsigned)instruction.instruction.operand.operand1[0], \
         (unsigned)instruction.instruction.operand.operand1[1]
  printf "closure=%p function=%p stackSize=%u instructionsLength=%u functionName=%p\n", \
         (void *)closure, (void *)closure->function, (unsigned)closure->function->stackSize, \
         (unsigned)closure->function->instructionsLength, (void *)closure->function->functionName
  printf "base=%p stackTop=%p functionBase=%p functionTop=%p\n", \
         (void *)base, (void *)state->stackTop.valuePointer, \
         (void *)callInfo->functionBase.valuePointer, (void *)callInfo->functionTop.valuePointer
  printf "opA=%p type=%d gc=%d native=%d int=%lld uint=%llu obj=%p\n", \
         (void *)opA, (int)opA->type, (int)opA->isGarbageCollectable, (int)opA->isNative, \
         (long long)opA->value.nativeObject.nativeInt64, (unsigned long long)opA->value.nativeObject.nativeUInt64, \
         (void *)opA->value.object
  printf "opB=%p type=%d gc=%d native=%d int=%lld uint=%llu obj=%p\n", \
         (void *)opB, (int)opB->type, (int)opB->isGarbageCollectable, (int)opB->isNative, \
         (long long)opB->value.nativeObject.nativeInt64, (unsigned long long)opB->value.nativeObject.nativeUInt64, \
         (void *)opB->value.object
  bt 12
  quit
end

run
