/* VM observer, PC-map trace decoder, and path assertions. */

static TZrDebugSignal record_vm_instruction(
        SZrState *state,
        SZrFunction *function,
        const TZrInstruction *programCounter,
        TZrUInt32 instructionOffset,
        TZrUInt32 line,
        TZrPtr userData) {
    SZrTestExecBcTrace *trace = (SZrTestExecBcTrace *)userData;
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(line);
    if (trace == ZR_NULL || programCounter == ZR_NULL ||
        function != trace->expectedFunction ||
        instructionOffset >= function->instructionsLength ||
        programCounter != &function->instructionsList[instructionOffset]) {
        if (trace != ZR_NULL) {
            trace->invalidProgramCounter = ZR_TRUE;
        }
        return ZR_DEBUG_SIGNAL_NONE;
    }
    if (trace->count >= ZR_TEST_EXECBC_TRACE_CAPACITY) {
        trace->overflow = ZR_TRUE;
        return ZR_DEBUG_SIGNAL_NONE;
    }
    trace->offsets[trace->count++] = instructionOffset;
    return ZR_DEBUG_SIGNAL_NONE;
}

static const SZrExecBcVmPcMapEntry *find_pc_map_entry(
        const SZrExecBcVmEmission *emission,
        TZrUInt32 pc) {
    TZrUInt32 index;
    if (emission == ZR_NULL || emission->pcMap == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0u; index < emission->pcMapCount; ++index) {
        if (emission->pcMap[index].pc == pc) {
            return &emission->pcMap[index];
        }
    }
    return ZR_NULL;
}

static void collect_vm_trace(
        const SZrTestExecBcTrace *trace,
        const SZrExecBcVmEmission *emission,
        TZrUInt32 originalBlockCount,
        TZrExecIrSourceId expectedSource,
        SZrTestExecBcReport *report) {
    TZrUInt32 index;

    report->pcMapValid = ZR_TRUE;
    for (index = 0u; index < trace->count; ++index) {
        const SZrExecBcVmPcMapEntry *entry =
                find_pc_map_entry(emission, trace->offsets[index]);
        if (entry == ZR_NULL || entry->blockId == 0u) {
            report->pcMapValid = ZR_FALSE;
            continue;
        }
        if (entry->sourceId == expectedSource) {
            report->sawExpectedSource = ZR_TRUE;
        }
        if (entry->blockId > originalBlockCount) {
            report->sawSyntheticBlock = ZR_TRUE;
        }
        if (report->pathCount == 0u ||
            report->path[report->pathCount - 1u] != entry->blockId) {
            if (report->pathCount >= ZR_TEST_EXECBC_PATH_CAPACITY) {
                report->pcMapValid = ZR_FALSE;
            } else {
                report->path[report->pathCount++] = entry->blockId;
            }
        }
    }
}

static TZrBool path_equals(
        const SZrTestExecBcReport *report,
        const TZrUInt32 *expected,
        TZrUInt32 expectedCount) {
    TZrUInt32 index;
    if (report == ZR_NULL || expected == ZR_NULL ||
        report->pathCount != expectedCount) {
        return ZR_FALSE;
    }
    for (index = 0u; index < expectedCount; ++index) {
        if (report->path[index] != expected[index]) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}
