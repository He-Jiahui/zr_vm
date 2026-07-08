#include "backend_aot_c_reference_locals.h"

static TZrBool backend_aot_c_reference_locals_instruction_writes_reference(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(TO_STRING):
        case ZR_INSTRUCTION_ENUM(TO_OBJECT):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_c_reference_locals_slot_declared_before(const SZrFunction *function,
                                                                   TZrUInt32 instructionLimit,
                                                                   TZrUInt32 slot) {
    TZrUInt32 instructionIndex;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (instructionIndex = 0u;
         instructionIndex < instructionLimit && instructionIndex < function->instructionsLength;
         instructionIndex++) {
        const TZrInstruction *instruction = &function->instructionsList[instructionIndex];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;

        if (backend_aot_c_reference_locals_instruction_writes_reference(opcode) &&
            instruction->instruction.operandExtra == slot) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool backend_aot_c_reference_locals_write_fields(FILE *file, const SZrFunction *function) {
    TZrUInt32 instructionIndex;
    TZrBool emittedAny = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (instructionIndex = 0u; instructionIndex < function->instructionsLength; instructionIndex++) {
        const TZrInstruction *instruction = &function->instructionsList[instructionIndex];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        TZrUInt32 slot = instruction->instruction.operandExtra;

        if (!backend_aot_c_reference_locals_instruction_writes_reference(opcode) ||
            backend_aot_c_reference_locals_slot_declared_before(function, instructionIndex, slot)) {
            continue;
        }

        emittedAny = ZR_TRUE;
        if (file != ZR_NULL) {
            fprintf(file, "    SZrRawObject *o%u;\n", (unsigned)slot);
        }
    }

    return emittedAny;
}

static TZrUInt32 backend_aot_c_reference_locals_count_fields(const SZrFunction *function) {
    TZrUInt32 instructionIndex;
    TZrUInt32 fieldCount = 0u;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return 0u;
    }

    for (instructionIndex = 0u; instructionIndex < function->instructionsLength; instructionIndex++) {
        const TZrInstruction *instruction = &function->instructionsList[instructionIndex];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        TZrUInt32 slot = instruction->instruction.operandExtra;

        if (!backend_aot_c_reference_locals_instruction_writes_reference(opcode) ||
            backend_aot_c_reference_locals_slot_declared_before(function, instructionIndex, slot)) {
            continue;
        }

        fieldCount++;
    }

    return fieldCount;
}

TZrBool backend_aot_c_reference_locals_has_locals(const SZrAotExecIrFunction *functionIr) {
    return (TZrBool)(functionIr != ZR_NULL &&
                     backend_aot_c_reference_locals_count_fields(functionIr->function) > 0u);
}

void backend_aot_write_c_reference_local_structs(FILE *file, const SZrAotFunctionTable *table) {
    TZrUInt32 entryIndex;

    if (file == ZR_NULL || table == ZR_NULL || table->entries == ZR_NULL) {
        return;
    }

    for (entryIndex = 0u; entryIndex < table->count; entryIndex++) {
        const SZrAotFunctionEntry *entry = &table->entries[entryIndex];

        if (entry->function == ZR_NULL ||
            !backend_aot_c_reference_locals_write_fields(ZR_NULL, entry->function)) {
            continue;
        }

        fprintf(file,
                "typedef struct SZrAotReferenceLocals_%u {\n",
                (unsigned)entry->flatIndex);
        backend_aot_c_reference_locals_write_fields(file, entry->function);
        fprintf(file,
                "} SZrAotReferenceLocals_%u;\n\n",
                (unsigned)entry->flatIndex);
    }
}

void backend_aot_write_c_reference_local_root_maps(FILE *file, const SZrAotFunctionTable *table) {
    TZrUInt32 entryIndex;

    if (file == ZR_NULL || table == ZR_NULL || table->entries == ZR_NULL) {
        return;
    }

    for (entryIndex = 0u; entryIndex < table->count; entryIndex++) {
        const SZrAotFunctionEntry *entry = &table->entries[entryIndex];
        const SZrFunction *function = entry->function;
        TZrUInt32 rootCount = backend_aot_c_reference_locals_count_fields(function);
        TZrUInt32 instructionIndex;

        if (function == ZR_NULL || function->instructionsList == ZR_NULL || rootCount == 0u) {
            continue;
        }

        fprintf(file, "static const SZrAotGcRootSlot zr_aot_ref_root_slots_%u[] = {\n",
                (unsigned)entry->flatIndex);
        for (instructionIndex = 0u; instructionIndex < function->instructionsLength; instructionIndex++) {
            const TZrInstruction *instruction = &function->instructionsList[instructionIndex];
            EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
            TZrUInt32 slot = instruction->instruction.operandExtra;

            if (!backend_aot_c_reference_locals_instruction_writes_reference(opcode) ||
                backend_aot_c_reference_locals_slot_declared_before(function, instructionIndex, slot)) {
                continue;
            }

            fprintf(file,
                    "    {\n"
                    "        .stackSlot = %uu,\n"
                    "        .frameByteOffset = (TZrUInt32)offsetof(SZrAotReferenceLocals_%u, o%u),\n"
                    "        .typeLayoutId = 0u,\n"
                    "        .fieldByteOffset = 0u,\n"
                    "        .locationKind = (TZrUInt8)ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS,\n"
                    "    },\n",
                    (unsigned)slot,
                    (unsigned)entry->flatIndex,
                    (unsigned)slot);
        }
        fprintf(file, "};\n");
        fprintf(file, "static const SZrAotGcRootMap zr_aot_ref_root_map_%u = {\n",
                (unsigned)entry->flatIndex);
        fprintf(file, "    %uu,\n", (unsigned)rootCount);
        fprintf(file, "    zr_aot_ref_root_slots_%u,\n", (unsigned)entry->flatIndex);
        fprintf(file, "};\n\n");
    }
}

void backend_aot_write_c_reference_locals(FILE *file, const SZrAotExecIrFunction *functionIr) {
    if (file == ZR_NULL || functionIr == ZR_NULL || functionIr->function == ZR_NULL ||
        !backend_aot_c_reference_locals_write_fields(ZR_NULL, functionIr->function)) {
        return;
    }

    fprintf(file,
            "    SZrAotReferenceLocals_%u zr_aot_ref_locals = { ZR_NULL };\n",
            (unsigned)functionIr->flatIndex);
}

void backend_aot_write_c_reference_local_root_frame_declaration(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    SZrAotGcRootFrame zr_aot_ref_gc_root_frame;\n"
            "    TZrBool zr_aot_has_ref_gc_root_frame = ZR_FALSE;\n");
}

void backend_aot_write_c_reference_local_root_frame_push(FILE *file, const SZrAotExecIrFunction *functionIr) {
    if (file == ZR_NULL || !backend_aot_c_reference_locals_has_locals(functionIr)) {
        return;
    }

    fprintf(file,
            "    /* zr_aot_reference_local_root_frame_push */\n"
            "    {\n"
            "        ZR_AOT_C_GUARD(ZrCore_Gc_AotRootFramePush(state,\n"
            "                                                  &zr_aot_ref_gc_root_frame,\n"
            "                                                  (TZrStackValuePointer)(void *)&zr_aot_ref_locals,\n"
            "                                                  &zr_aot_ref_root_map_%u));\n"
            "        zr_aot_has_ref_gc_root_frame = ZR_TRUE;\n"
            "    }\n",
            (unsigned)functionIr->flatIndex);
}

void backend_aot_write_c_reference_local_root_frame_cleanup(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    if (zr_aot_has_ref_gc_root_frame) {\n"
            "        ZrCore_Gc_AotRootFramePop(state, &zr_aot_ref_gc_root_frame);\n"
            "        zr_aot_has_ref_gc_root_frame = ZR_FALSE;\n"
            "    }\n");
}
