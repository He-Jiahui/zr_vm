set pagination off
set confirm off
set breakpoint pending on

file ./build/codex-wsl-gcc-debug/bin/zr_vm_cli
set args ./tests/benchmarks/cases/matrix_add_2d/zr/benchmark_matrix_add_2d.zrp --execution-mode aot_c --require-aot-path

break ZrLibrary_AotRuntime_ModSignedConst if destinationSlot == 21
disable 1

break ZrLibrary_AotRuntime_ToString if sourceSlot == 21
commands
silent
printf "\nTO_STRING ins=%u source=%u\n", frame->currentInstructionIndex, sourceSlot
printf "  slot21 final: type=%d int=%lld\n", (frame->slotBase + 21)->value.type, (long long)(frame->slotBase + 21)->value.value.nativeObject.nativeInt64
printf "  generated slots=%u\n", frame->generatedFrameSlotCount
printf "  function span=%lld\n", (long long)(frame->callInfo->functionTop.valuePointer - frame->callInfo->functionBase.valuePointer - 1)
continue
end

break ZrLibrary_AotRuntime_BeginInstruction if instructionIndex == 103 || instructionIndex == 104
commands
silent
printf "\nBEGIN ins=%u\n", instructionIndex
printf "  slot11 type=%d obj=%p len=%u head=%c%c%c%c%c%c%c%c\n", (frame->slotBase + 11)->value.type, (frame->slotBase + 11)->value.value.object, ((SZrString *)(frame->slotBase + 11)->value.value.object)->shortStringLength, ((SZrString *)(frame->slotBase + 11)->value.value.object)->stringDataExtend[0], ((SZrString *)(frame->slotBase + 11)->value.value.object)->stringDataExtend[1], ((SZrString *)(frame->slotBase + 11)->value.value.object)->stringDataExtend[2], ((SZrString *)(frame->slotBase + 11)->value.value.object)->stringDataExtend[3], ((SZrString *)(frame->slotBase + 11)->value.value.object)->stringDataExtend[4], ((SZrString *)(frame->slotBase + 11)->value.value.object)->stringDataExtend[5], ((SZrString *)(frame->slotBase + 11)->value.value.object)->stringDataExtend[6], ((SZrString *)(frame->slotBase + 11)->value.value.object)->stringDataExtend[7]
printf "  slot12 type=%d obj=%p len=%u head=%c%c%c%c%c%c%c%c\n", (frame->slotBase + 12)->value.type, (frame->slotBase + 12)->value.value.object, ((SZrString *)(frame->slotBase + 12)->value.value.object)->shortStringLength, ((SZrString *)(frame->slotBase + 12)->value.value.object)->stringDataExtend[0], ((SZrString *)(frame->slotBase + 12)->value.value.object)->stringDataExtend[1], ((SZrString *)(frame->slotBase + 12)->value.value.object)->stringDataExtend[2], ((SZrString *)(frame->slotBase + 12)->value.value.object)->stringDataExtend[3], ((SZrString *)(frame->slotBase + 12)->value.value.object)->stringDataExtend[4], ((SZrString *)(frame->slotBase + 12)->value.value.object)->stringDataExtend[5], ((SZrString *)(frame->slotBase + 12)->value.value.object)->stringDataExtend[6], ((SZrString *)(frame->slotBase + 12)->value.value.object)->stringDataExtend[7]
continue
end

run
