//
// Minimal SemIR -> AOTIR -> textual backend lowering.
//

#include "zr_vm_parser/writer.h"

#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"

#include <stdio.h>
#include <string.h>

#define ZR_AOT_COUNT_NONE 0U
#define ZR_AOT_FUNCTION_TREE_ROOT_INDEX 0U
#define ZR_AOT_EMBEDDED_BLOB_EMPTY_BYTE 0x00u

typedef enum EZrAotRuntimeContract {
    ZR_AOT_RUNTIME_CONTRACT_NONE = 0,
    ZR_AOT_RUNTIME_CONTRACT_REFLECTION_TYPEOF = 1 << 0,
    ZR_AOT_RUNTIME_CONTRACT_FUNCTION_PRECALL = 1 << 1,
    ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_SHARE = 1 << 2,
    ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_WEAK = 1 << 3,
    ZR_AOT_RUNTIME_CONTRACT_ITER_INIT = 1 << 4,
    ZR_AOT_RUNTIME_CONTRACT_ITER_MOVE_NEXT = 1 << 5,
    ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_UPGRADE = 1 << 6,
    ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_RELEASE = 1 << 7
} EZrAotRuntimeContract;

typedef struct SZrAotIrInstruction {
    TZrUInt32 semIrOpcode;
    TZrUInt32 execInstructionIndex;
    TZrUInt32 typeTableIndex;
    TZrUInt32 effectTableIndex;
    TZrUInt32 destinationSlot;
    TZrUInt32 operand0;
    TZrUInt32 operand1;
    TZrUInt32 deoptId;
} SZrAotIrInstruction;

typedef struct SZrAotIrModule {
    SZrAotIrInstruction *instructions;
    TZrUInt32 instructionCount;
    TZrUInt32 runtimeContracts;
} SZrAotIrModule;

static const TZrChar *backend_aot_semir_opcode_name(TZrUInt32 opcode) {
    switch ((EZrSemIrOpcode)opcode) {
        case ZR_SEMIR_OPCODE_OWN_UNIQUE:
            return "OWN_UNIQUE";
        case ZR_SEMIR_OPCODE_OWN_USING:
            return "OWN_USING";
        case ZR_SEMIR_OPCODE_OWN_SHARE:
            return "OWN_SHARE";
        case ZR_SEMIR_OPCODE_OWN_WEAK:
            return "OWN_WEAK";
        case ZR_SEMIR_OPCODE_OWN_UPGRADE:
            return "OWN_UPGRADE";
        case ZR_SEMIR_OPCODE_OWN_RELEASE:
            return "OWN_RELEASE";
        case ZR_SEMIR_OPCODE_TYPEOF:
            return "TYPEOF";
        case ZR_SEMIR_OPCODE_DYN_CALL:
            return "DYN_CALL";
        case ZR_SEMIR_OPCODE_DYN_TAIL_CALL:
            return "DYN_TAIL_CALL";
        case ZR_SEMIR_OPCODE_META_CALL:
            return "META_CALL";
        case ZR_SEMIR_OPCODE_META_TAIL_CALL:
            return "META_TAIL_CALL";
        case ZR_SEMIR_OPCODE_META_GET:
            return "META_GET";
        case ZR_SEMIR_OPCODE_META_SET:
            return "META_SET";
        case ZR_SEMIR_OPCODE_DYN_ITER_INIT:
            return "DYN_ITER_INIT";
        case ZR_SEMIR_OPCODE_DYN_ITER_MOVE_NEXT:
            return "DYN_ITER_MOVE_NEXT";
        case ZR_SEMIR_OPCODE_NOP:
        default:
            return "NOP";
    }
}

static TZrUInt32 backend_aot_runtime_contracts_for_opcode(TZrUInt32 opcode) {
    switch ((EZrSemIrOpcode)opcode) {
        case ZR_SEMIR_OPCODE_TYPEOF:
            return ZR_AOT_RUNTIME_CONTRACT_REFLECTION_TYPEOF;
        case ZR_SEMIR_OPCODE_DYN_CALL:
        case ZR_SEMIR_OPCODE_DYN_TAIL_CALL:
        case ZR_SEMIR_OPCODE_META_CALL:
        case ZR_SEMIR_OPCODE_META_TAIL_CALL:
        case ZR_SEMIR_OPCODE_META_GET:
        case ZR_SEMIR_OPCODE_META_SET:
            return ZR_AOT_RUNTIME_CONTRACT_FUNCTION_PRECALL;
        case ZR_SEMIR_OPCODE_DYN_ITER_INIT:
            return ZR_AOT_RUNTIME_CONTRACT_ITER_INIT;
        case ZR_SEMIR_OPCODE_DYN_ITER_MOVE_NEXT:
            return ZR_AOT_RUNTIME_CONTRACT_ITER_MOVE_NEXT;
        case ZR_SEMIR_OPCODE_OWN_SHARE:
            return ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_SHARE;
        case ZR_SEMIR_OPCODE_OWN_WEAK:
            return ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_WEAK;
        case ZR_SEMIR_OPCODE_OWN_UPGRADE:
            return ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_UPGRADE;
        case ZR_SEMIR_OPCODE_OWN_RELEASE:
            return ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_RELEASE;
        case ZR_SEMIR_OPCODE_OWN_UNIQUE:
        case ZR_SEMIR_OPCODE_OWN_USING:
        case ZR_SEMIR_OPCODE_NOP:
        default:
            return ZR_AOT_RUNTIME_CONTRACT_NONE;
    }
}

static void backend_aot_release_module(SZrState *state, SZrAotIrModule *module) {
    if (state == ZR_NULL || state->global == ZR_NULL || module == ZR_NULL) {
        return;
    }

    if (module->instructions != ZR_NULL && module->instructionCount > 0) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      module->instructions,
                                      sizeof(SZrAotIrInstruction) * module->instructionCount,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    module->instructions = ZR_NULL;
    module->instructionCount = 0;
    module->runtimeContracts = ZR_AOT_RUNTIME_CONTRACT_NONE;
}

static TZrUInt32 backend_aot_count_function_tree_instructions(const SZrFunction *function) {
    TZrUInt32 childIndex;
    TZrUInt32 count = ZR_AOT_COUNT_NONE;

    if (function == ZR_NULL) {
        return ZR_AOT_COUNT_NONE;
    }

    count += function->semIrInstructionLength;
    for (childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        count += backend_aot_count_function_tree_instructions(&function->childFunctionList[childIndex]);
    }

    return count;
}

static void backend_aot_collect_function_tree(const SZrFunction *function,
                                              SZrAotIrInstruction *instructions,
                                              TZrUInt32 instructionCapacity,
                                              TZrUInt32 *ioInstructionIndex,
                                              TZrUInt32 *ioRuntimeContracts) {
    TZrUInt32 index;
    TZrUInt32 childIndex;

    if (function == ZR_NULL || instructions == ZR_NULL || ioInstructionIndex == ZR_NULL ||
        ioRuntimeContracts == ZR_NULL) {
        return;
    }

    for (index = 0; index < function->semIrInstructionLength && *ioInstructionIndex < instructionCapacity; index++) {
        const SZrSemIrInstruction *src = &function->semIrInstructions[index];
        SZrAotIrInstruction *dst = &instructions[*ioInstructionIndex];

        dst->semIrOpcode = src->opcode;
        dst->execInstructionIndex = src->execInstructionIndex;
        dst->typeTableIndex = src->typeTableIndex;
        dst->effectTableIndex = src->effectTableIndex;
        dst->destinationSlot = src->destinationSlot;
        dst->operand0 = src->operand0;
        dst->operand1 = src->operand1;
        dst->deoptId = src->deoptId;
        *ioRuntimeContracts |= backend_aot_runtime_contracts_for_opcode(src->opcode);
        (*ioInstructionIndex)++;
    }

    for (childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        backend_aot_collect_function_tree(&function->childFunctionList[childIndex],
                                          instructions,
                                          instructionCapacity,
                                          ioInstructionIndex,
                                          ioRuntimeContracts);
    }
}

static TZrBool backend_aot_build_module(SZrState *state, SZrFunction *function, SZrAotIrModule *outModule) {
    TZrUInt32 totalInstructionCount;
    TZrUInt32 writeIndex = 0;

    if (state == ZR_NULL || state->global == ZR_NULL || function == ZR_NULL || outModule == ZR_NULL) {
        return ZR_FALSE;
    }

    outModule->instructions = ZR_NULL;
    outModule->instructionCount = 0;
    outModule->runtimeContracts = ZR_AOT_RUNTIME_CONTRACT_NONE;

    totalInstructionCount = backend_aot_count_function_tree_instructions(function);
    if (totalInstructionCount == 0) {
        return ZR_TRUE;
    }

    outModule->instructions = (SZrAotIrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrAotIrInstruction) * totalInstructionCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (outModule->instructions == ZR_NULL) {
        return ZR_FALSE;
    }

    outModule->instructionCount = totalInstructionCount;
    ZrCore_Memory_RawSet(outModule->instructions,
                         0,
                         sizeof(SZrAotIrInstruction) * outModule->instructionCount);

    backend_aot_collect_function_tree(function,
                                      outModule->instructions,
                                      outModule->instructionCount,
                                      &writeIndex,
                                      &outModule->runtimeContracts);

    return ZR_TRUE;
}

static void backend_aot_write_c_contracts(FILE *file, TZrUInt32 runtimeContracts) {
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_REFLECTION_TYPEOF) {
        fprintf(file, "/* runtime contract: ZrCore_Reflection_TypeOfValue */\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_FUNCTION_PRECALL) {
        fprintf(file, "/* runtime contract: ZrCore_Function_PreCall */\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_SHARE) {
        fprintf(file, "/* runtime contract: ZrCore_Ownership_NativeShared */\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_WEAK) {
        fprintf(file, "/* runtime contract: ZrCore_Ownership_NativeWeak */\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_UPGRADE) {
        fprintf(file, "/* runtime contract: ZrCore_Ownership_UpgradeValue */\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_RELEASE) {
        fprintf(file, "/* runtime contract: ZrCore_Ownership_ReleaseValue */\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_ITER_INIT) {
        fprintf(file, "/* runtime contract: ZrCore_Object_IterInit */\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_ITER_MOVE_NEXT) {
        fprintf(file, "/* runtime contract: ZrCore_Object_IterMoveNext */\n");
    }
}

static void backend_aot_write_llvm_contracts(FILE *file, TZrUInt32 runtimeContracts) {
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_REFLECTION_TYPEOF) {
        fprintf(file, "declare i1 @ZrCore_Reflection_TypeOfValue(ptr, ptr, ptr)\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_FUNCTION_PRECALL) {
        fprintf(file, "declare ptr @ZrCore_Function_PreCall(ptr, ptr, i64, ptr)\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_SHARE) {
        fprintf(file, "declare i1 @ZrCore_Ownership_NativeShared(ptr)\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_WEAK) {
        fprintf(file, "declare i1 @ZrCore_Ownership_NativeWeak(ptr)\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_UPGRADE) {
        fprintf(file, "declare i1 @ZrCore_Ownership_UpgradeValue(ptr, ptr, ptr)\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_RELEASE) {
        fprintf(file, "declare void @ZrCore_Ownership_ReleaseValue(ptr, ptr)\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_ITER_INIT) {
        fprintf(file, "declare i1 @ZrCore_Object_IterInit(ptr, ptr, ptr)\n");
    }
    if (runtimeContracts & ZR_AOT_RUNTIME_CONTRACT_ITER_MOVE_NEXT) {
        fprintf(file, "declare i1 @ZrCore_Object_IterMoveNext(ptr, ptr, ptr)\n");
    }
}

static void backend_aot_write_instruction_listing(FILE *file,
                                                  const TZrChar *prefix,
                                                  const SZrAotIrModule *module) {
    TZrUInt32 index;

    if (file == ZR_NULL || module == ZR_NULL) {
        return;
    }

    for (index = 0; index < module->instructionCount; index++) {
        const SZrAotIrInstruction *instruction = &module->instructions[index];
        fprintf(file,
                "%s[%u] %s exec=%u type=%u effect=%u dst=%u op0=%u op1=%u deopt=%u\n",
                prefix,
                index,
                backend_aot_semir_opcode_name(instruction->semIrOpcode),
                instruction->execInstructionIndex,
                instruction->typeTableIndex,
                instruction->effectTableIndex,
                instruction->destinationSlot,
                instruction->operand0,
                instruction->operand1,
                instruction->deoptId);
    }
}

static const TZrChar *backend_aot_option_text(const SZrAotWriterOptions *options,
                                              const TZrChar *candidate,
                                              const TZrChar *fallback) {
    ZR_UNUSED_PARAMETER(options);
    return (candidate != ZR_NULL && candidate[0] != '\0') ? candidate : fallback;
}

static const TZrChar *backend_aot_runtime_contract_name(TZrUInt32 contractBit) {
    switch (contractBit) {
        case ZR_AOT_RUNTIME_CONTRACT_REFLECTION_TYPEOF:
            return "reflection.typeof";
        case ZR_AOT_RUNTIME_CONTRACT_FUNCTION_PRECALL:
            return "dispatch.precall";
        case ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_SHARE:
            return "ownership.share";
        case ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_WEAK:
            return "ownership.weak";
        case ZR_AOT_RUNTIME_CONTRACT_ITER_INIT:
            return "iter.init";
        case ZR_AOT_RUNTIME_CONTRACT_ITER_MOVE_NEXT:
            return "iter.move_next";
        case ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_UPGRADE:
            return "ownership.upgrade";
        case ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_RELEASE:
            return "ownership.release";
        case ZR_AOT_RUNTIME_CONTRACT_NONE:
        default:
            return "none";
    }
}

static void backend_aot_write_runtime_contract_array_c(FILE *file, TZrUInt32 runtimeContracts) {
    TZrUInt32 contractBit;

    fprintf(file, "static const TZrChar *const zr_aot_runtime_contracts[] = {\n");
    for (contractBit = 1; contractBit <= ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_RELEASE; contractBit <<= 1) {
        if ((runtimeContracts & contractBit) == 0) {
            continue;
        }
        fprintf(file, "    \"%s\",\n", backend_aot_runtime_contract_name(contractBit));
    }
    fprintf(file, "    ZR_NULL,\n");
    fprintf(file, "};\n");
}

static void backend_aot_write_runtime_contract_array_llvm(FILE *file, TZrUInt32 runtimeContracts) {
    TZrUInt32 contractBit;

    fprintf(file, "; runtimeContracts:");
    for (contractBit = 1; contractBit <= ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_RELEASE; contractBit <<= 1) {
        if ((runtimeContracts & contractBit) == 0) {
            continue;
        }
        fprintf(file, " %s", backend_aot_runtime_contract_name(contractBit));
    }
    fprintf(file, "\n");
}

static TZrUInt32 backend_aot_runtime_contract_count(TZrUInt32 runtimeContracts) {
    TZrUInt32 contractBit;
    TZrUInt32 count = ZR_AOT_COUNT_NONE;

    for (contractBit = 1; contractBit <= ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_RELEASE; contractBit <<= 1) {
        if ((runtimeContracts & contractBit) != 0) {
            count++;
        }
    }

    return count;
}

static void backend_aot_write_runtime_contract_globals_llvm(FILE *file, TZrUInt32 runtimeContracts) {
    TZrUInt32 contractBit;
    TZrUInt32 contractCount;
    TZrUInt32 contractIndex = 0;

    if (file == ZR_NULL) {
        return;
    }

    contractCount = backend_aot_runtime_contract_count(runtimeContracts);
    for (contractBit = 1; contractBit <= ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_RELEASE; contractBit <<= 1) {
        const TZrChar *contractName;

        if ((runtimeContracts & contractBit) == 0) {
            continue;
        }

        contractName = backend_aot_runtime_contract_name(contractBit);
        fprintf(file,
                "@zr_aot_runtime_contract_%u = private unnamed_addr constant [%llu x i8] c\"%s\\00\"\n",
                (unsigned)contractIndex,
                (unsigned long long)(strlen(contractName) + 1),
                contractName);
        contractIndex++;
    }

    fprintf(file, "@zr_aot_runtime_contracts = private constant [%u x ptr] [", (unsigned)(contractCount + 1));
    contractIndex = 0;
    for (contractBit = 1; contractBit <= ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_RELEASE; contractBit <<= 1) {
        if ((runtimeContracts & contractBit) == 0) {
            continue;
        }

        if (contractIndex > 0) {
            fprintf(file, ", ");
        }
        fprintf(file, "ptr @zr_aot_runtime_contract_%u", (unsigned)contractIndex);
        contractIndex++;
    }
    if (contractIndex > 0) {
        fprintf(file, ", ");
    }
    fprintf(file, "ptr null]\n");
}

typedef struct SZrAotCFunctionEntry {
    const SZrFunction *function;
    TZrUInt32 flatIndex;
} SZrAotCFunctionEntry;

typedef struct SZrAotCFunctionTable {
    SZrAotCFunctionEntry *entries;
    TZrUInt32 count;
    TZrUInt32 capacity;
} SZrAotCFunctionTable;

static const SZrFunction *backend_aot_function_from_constant_value(SZrState *state, const SZrTypeValue *value) {
    if (state == ZR_NULL || value == ZR_NULL || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->type == ZR_VALUE_TYPE_FUNCTION &&
        !value->isNative &&
        value->value.object->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
        return ZR_CAST_FUNCTION(state, value->value.object);
    }

    if (value->type == ZR_VALUE_TYPE_CLOSURE &&
        !value->isNative &&
        value->value.object->type == ZR_RAW_OBJECT_TYPE_CLOSURE) {
        SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, value->value.object);
        return closure != ZR_NULL ? closure->function : ZR_NULL;
    }

    return ZR_NULL;
}

static TZrUInt32 backend_aot_count_function_graph_capacity(SZrState *state, const SZrFunction *function) {
    TZrUInt32 childIndex;
    TZrUInt32 constantIndex;
    TZrUInt32 count = ZR_AOT_COUNT_NONE;

    if (state == ZR_NULL || function == ZR_NULL) {
        return ZR_AOT_COUNT_NONE;
    }

    count = ZR_AOT_FUNCTION_TREE_ROOT_INDEX + 1U;
    for (constantIndex = 0; constantIndex < function->constantValueLength; constantIndex++) {
        const SZrFunction *constantFunction =
                backend_aot_function_from_constant_value(state, &function->constantValueList[constantIndex]);
        if (constantFunction != ZR_NULL) {
            count += backend_aot_count_function_graph_capacity(state, constantFunction);
        }
    }

    for (childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        count += backend_aot_count_function_graph_capacity(state, &function->childFunctionList[childIndex]);
    }

    return count;
}

static TZrBool backend_aot_functions_equivalent(const SZrFunction *left, const SZrFunction *right) {
    TZrBool sameFunctionName;

    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }

    sameFunctionName = left->functionName == right->functionName ||
                       (left->functionName == ZR_NULL && right->functionName == ZR_NULL) ||
                       (left->functionName != ZR_NULL && right->functionName != ZR_NULL &&
                        ZrCore_String_Equal(left->functionName, right->functionName));

    return sameFunctionName &&
           left->parameterCount == right->parameterCount &&
           left->instructionsLength == right->instructionsLength &&
           left->lineInSourceStart == right->lineInSourceStart &&
           left->lineInSourceEnd == right->lineInSourceEnd;
}

static TZrBool backend_aot_function_table_contains(const SZrAotCFunctionEntry *entries,
                                                   TZrUInt32 count,
                                                   const SZrFunction *function) {
    TZrUInt32 index;

    if (entries == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < count; index++) {
        if (entries[index].function == function ||
            backend_aot_functions_equivalent(entries[index].function, function)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void backend_aot_flatten_function_graph(SZrState *state,
                                               const SZrFunction *function,
                                               SZrAotCFunctionEntry *entries,
                                               TZrUInt32 capacity,
                                               TZrUInt32 *ioIndex) {
    TZrUInt32 childIndex;
    TZrUInt32 constantIndex;

    if (state == ZR_NULL || function == ZR_NULL || entries == ZR_NULL || ioIndex == ZR_NULL) {
        return;
    }

    if (backend_aot_function_table_contains(entries, *ioIndex, function)) {
        return;
    }
    if (*ioIndex >= capacity) {
        return;
    }

    entries[*ioIndex].function = function;
    entries[*ioIndex].flatIndex = *ioIndex;
    (*ioIndex)++;

    for (constantIndex = 0; constantIndex < function->constantValueLength; constantIndex++) {
        const SZrFunction *constantFunction =
                backend_aot_function_from_constant_value(state, &function->constantValueList[constantIndex]);
        if (constantFunction != ZR_NULL) {
            backend_aot_flatten_function_graph(state, constantFunction, entries, capacity, ioIndex);
        }
    }

    for (childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        backend_aot_flatten_function_graph(state, &function->childFunctionList[childIndex], entries, capacity, ioIndex);
    }
}

static TZrBool backend_aot_build_c_function_table(SZrState *state,
                                                  const SZrFunction *function,
                                                  SZrAotCFunctionTable *outTable) {
    TZrUInt32 capacity;
    TZrUInt32 writeIndex = 0;

    if (state == ZR_NULL || state->global == ZR_NULL || function == ZR_NULL || outTable == ZR_NULL) {
        return ZR_FALSE;
    }

    outTable->entries = ZR_NULL;
    outTable->count = ZR_AOT_COUNT_NONE;
    outTable->capacity = ZR_AOT_COUNT_NONE;

    ZrCore_Function_RebindConstantFunctionValuesToChildren((SZrFunction *)function);
    capacity = backend_aot_count_function_graph_capacity(state, function);
    if (capacity == 0) {
        return ZR_TRUE;
    }

    outTable->entries = (SZrAotCFunctionEntry *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrAotCFunctionEntry) * capacity,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (outTable->entries == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(outTable->entries, 0, sizeof(SZrAotCFunctionEntry) * capacity);
    backend_aot_flatten_function_graph(state, function, outTable->entries, capacity, &writeIndex);
    outTable->count = writeIndex;
    outTable->capacity = capacity;
    return ZR_TRUE;
}

static void backend_aot_release_c_function_table(SZrState *state, SZrAotCFunctionTable *table) {
    if (state == ZR_NULL || state->global == ZR_NULL || table == ZR_NULL) {
        return;
    }

    if (table->entries != ZR_NULL && table->capacity > 0) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      table->entries,
                                      sizeof(SZrAotCFunctionEntry) * table->capacity,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    table->entries = ZR_NULL;
    table->count = ZR_AOT_COUNT_NONE;
    table->capacity = ZR_AOT_COUNT_NONE;
}

static TZrBool backend_aot_c_instruction_supported(const TZrInstruction *instruction) {
    TZrUInt16 opcode;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(GET_STACK):
        case ZR_INSTRUCTION_ENUM(SET_STACK):
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
        case ZR_INSTRUCTION_ENUM(TO_INT):
        case ZR_INSTRUCTION_ENUM(ADD_INT):
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
        case ZR_INSTRUCTION_ENUM(GET_MEMBER):
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
            return instruction->instruction.operand.operand1[1] == 0;
        default:
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_function_is_executable_subset(const SZrFunction *function) {
    TZrUInt32 instructionIndex;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    for (instructionIndex = 0; instructionIndex < function->instructionsLength; instructionIndex++) {
        if (!backend_aot_c_instruction_supported(&function->instructionsList[instructionIndex])) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_function_uses_vm_shim(const SZrAotCFunctionEntry *entry) {
    return entry == ZR_NULL || entry->function == ZR_NULL || !backend_aot_function_is_executable_subset(entry->function);
}

static const TZrChar *backend_aot_function_display_name(const SZrFunction *function) {
    if (function == ZR_NULL || function->functionName == ZR_NULL) {
        return "__anonymous__";
    }

    return ZrCore_String_GetNativeString(function->functionName);
}

static TZrBool backend_aot_report_first_unsupported_instruction(const TZrChar *moduleName,
                                                                const SZrAotCFunctionTable *table) {
    TZrUInt32 functionIndex;

    if (table == ZR_NULL) {
        return ZR_FALSE;
    }

    for (functionIndex = 0; functionIndex < table->count; functionIndex++) {
        const SZrAotCFunctionEntry *entry = &table->entries[functionIndex];
        TZrUInt32 instructionIndex;

        if (entry->function == ZR_NULL) {
            continue;
        }

        for (instructionIndex = 0; instructionIndex < entry->function->instructionsLength; instructionIndex++) {
            const TZrInstruction *instruction = &entry->function->instructionsList[instructionIndex];
            if (backend_aot_c_instruction_supported(instruction)) {
                continue;
            }

            fprintf(stderr,
                    "aot_c lowering unsupported: module='%s' function='%s' functionIndex=%u instructionIndex=%u opcode=%u\n",
                    moduleName != ZR_NULL ? moduleName : "__entry__",
                    backend_aot_function_display_name(entry->function),
                    (unsigned)entry->flatIndex,
                    (unsigned)instructionIndex,
                    (unsigned)instruction->instruction.operationCode);
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrUInt32 backend_aot_option_input_kind(const SZrAotWriterOptions *options) {
    return (options != ZR_NULL && options->inputKind != ZR_AOT_INPUT_KIND_NONE)
                   ? options->inputKind
                   : ZR_AOT_INPUT_KIND_SOURCE;
}

static const TZrChar *backend_aot_option_input_hash(const SZrAotWriterOptions *options,
                                                    const TZrChar *sourceHash,
                                                    const TZrChar *zroHash) {
    if (options != ZR_NULL && options->inputHash != ZR_NULL && options->inputHash[0] != '\0') {
        return options->inputHash;
    }

    if (options != ZR_NULL && options->inputKind == ZR_AOT_INPUT_KIND_BINARY) {
        return zroHash;
    }

    return sourceHash;
}

static void backend_aot_write_embedded_blob_c(FILE *file,
                                              const TZrByte *blob,
                                              TZrSize blobLength) {
    TZrSize index;

    fprintf(file, "static const TZrByte zr_aot_embedded_module_blob[] = {\n");
    if (blob != ZR_NULL && blobLength > 0) {
        for (index = 0; index < blobLength; index++) {
            if ((index % 12) == 0) {
                fprintf(file, "    ");
            }
            fprintf(file, "0x%02x", blob[index]);
            if (index + 1 < blobLength) {
                fprintf(file, ", ");
            }
            if ((index % 12) == 11 || index + 1 == blobLength) {
                fprintf(file, "\n");
            }
        }
    } else {
        fprintf(file, "    0x%02x\n", (unsigned)ZR_AOT_EMBEDDED_BLOB_EMPTY_BYTE);
    }
    fprintf(file, "};\n");
}

static void backend_aot_write_c_function_forward_decls(FILE *file, const SZrAotCFunctionTable *table) {
    TZrUInt32 index;

    if (file == ZR_NULL || table == ZR_NULL) {
        return;
    }

    for (index = 0; index < table->count; index++) {
        fprintf(file, "static TZrInt64 zr_aot_fn_%u(struct SZrState *state);\n", (unsigned)index);
    }
}

static void backend_aot_write_c_function_table(FILE *file, const SZrAotCFunctionTable *table) {
    TZrUInt32 index;

    if (file == ZR_NULL || table == ZR_NULL) {
        return;
    }

    fprintf(file, "static const FZrAotEntryThunk zr_aot_function_thunks[] = {\n");
    for (index = 0; index < table->count; index++) {
        fprintf(file, "    zr_aot_fn_%u,\n", (unsigned)index);
    }
    fprintf(file, "};\n");
}

static void backend_aot_write_c_guard_macro(FILE *file) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file, "#define ZR_AOT_C_GUARD(call_expr) \\\n");
    fprintf(file, "    do { \\\n");
    fprintf(file, "        if (!(call_expr)) { \\\n");
    fprintf(file, "            goto zr_aot_fail; \\\n");
    fprintf(file, "        } \\\n");
    fprintf(file, "    } while (0)\n");
}

static TZrBool backend_aot_constant_requires_materialization(SZrState *state,
                                                             const SZrFunction *function,
                                                             TZrInt32 constantIndex) {
    if (state == ZR_NULL || function == ZR_NULL || constantIndex < 0 ||
        (TZrUInt32)constantIndex >= function->constantValueLength) {
        return ZR_TRUE;
    }

    return backend_aot_function_from_constant_value(state, &function->constantValueList[(TZrUInt32)constantIndex]) !=
           ZR_NULL;
}

static void backend_aot_write_c_direct_constant_copy(FILE *file,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 constantIndex) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZrCore_Value_Copy(state,\n"
            "                      ZrCore_Stack_GetValue(frame.slotBase + %u),\n"
            "                      &frame.function->constantValueList[%u]);\n",
            (unsigned)destinationSlot,
            (unsigned)constantIndex);
}

static void backend_aot_write_c_direct_stack_copy(FILE *file,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    ZrCore_Value_Copy(state,\n"
            "                      ZrCore_Stack_GetValue(frame.slotBase + %u),\n"
            "                      ZrCore_Stack_GetValue(frame.slotBase + %u));\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
}

static void backend_aot_write_c_direct_add_int(FILE *file,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 leftSlot,
                                               TZrUInt32 rightSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_left = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_right = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        TZrInt64 zr_aot_left_int;\n"
            "        TZrInt64 zr_aot_right_int;\n"
            "        if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_left->type)) {\n"
            "            zr_aot_left_int = zr_aot_left->value.nativeObject.nativeInt64;\n"
            "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_left->type)) {\n"
            "            zr_aot_left_int = (TZrInt64)zr_aot_left->value.nativeObject.nativeUInt64;\n"
            "        } else {\n"
            "            goto zr_aot_fail;\n"
            "        }\n"
            "        if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_right->type)) {\n"
            "            zr_aot_right_int = zr_aot_right->value.nativeObject.nativeInt64;\n"
            "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_right->type)) {\n"
            "            zr_aot_right_int = (TZrInt64)zr_aot_right->value.nativeObject.nativeUInt64;\n"
            "        } else {\n"
            "            goto zr_aot_fail;\n"
            "        }\n"
            "        ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                          nativeInt64,\n"
            "                          zr_aot_left_int + zr_aot_right_int,\n"
            "                          ZR_VALUE_TYPE_INT64);\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)leftSlot,
            (unsigned)rightSlot);
}

static void backend_aot_write_c_direct_to_int(FILE *file,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        const SZrTypeValue *zr_aot_source = ZrCore_Stack_GetValue(frame.slotBase + %u);\n"
            "        if (ZR_VALUE_IS_TYPE_SIGNED_INT(zr_aot_source->type)) {\n"
            "            ZrCore_Value_Copy(state, zr_aot_destination, zr_aot_source);\n"
            "        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(zr_aot_source->type)) {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeInt64,\n"
            "                              (TZrInt64)zr_aot_source->value.nativeObject.nativeUInt64,\n"
            "                              ZR_VALUE_TYPE_INT64);\n"
            "        } else if (ZR_VALUE_IS_TYPE_FLOAT(zr_aot_source->type)) {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeInt64,\n"
            "                              (TZrInt64)zr_aot_source->value.nativeObject.nativeDouble,\n"
            "                              ZR_VALUE_TYPE_INT64);\n"
            "        } else if (ZR_VALUE_IS_TYPE_BOOL(zr_aot_source->type)) {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination,\n"
            "                              nativeInt64,\n"
            "                              zr_aot_source->value.nativeObject.nativeBool ? 1 : 0,\n"
            "                              ZR_VALUE_TYPE_INT64);\n"
            "        } else {\n"
            "            ZR_VALUE_FAST_SET(zr_aot_destination, nativeInt64, 0, ZR_VALUE_TYPE_INT64);\n"
            "        }\n"
            "    }\n",
            (unsigned)destinationSlot,
            (unsigned)sourceSlot);
}

static void backend_aot_write_c_direct_return(FILE *file, TZrUInt32 sourceSlot) {
    if (file == ZR_NULL) {
        return;
    }

    fprintf(file,
            "    {\n"
            "        SZrCallInfo *zr_aot_call_info = state->callInfoList;\n"
            "        TZrStackValuePointer zr_aot_result_slot = frame.slotBase + %u;\n"
            "        if (zr_aot_call_info == ZR_NULL || zr_aot_call_info->functionBase.valuePointer == ZR_NULL ||\n"
            "            zr_aot_result_slot == ZR_NULL) {\n"
            "            goto zr_aot_fail;\n"
            "        }\n"
            "        ZrCore_Value_Copy(state,\n"
            "                          ZrCore_Stack_GetValue(zr_aot_call_info->functionBase.valuePointer),\n"
            "                          ZrCore_Stack_GetValue(zr_aot_result_slot));\n"
            "        state->stackTop.valuePointer = zr_aot_call_info->functionBase.valuePointer + 1;\n"
            "        return 1;\n"
            "    }\n",
            (unsigned)sourceSlot);
}

static void backend_aot_write_c_function_body(FILE *file,
                                              SZrState *state,
                                              const SZrAotCFunctionEntry *entry) {
    TZrUInt32 instructionIndex;
    TZrBool publishExports;

    if (file == ZR_NULL || entry == ZR_NULL || entry->function == ZR_NULL) {
        return;
    }

    publishExports = entry->flatIndex == ZR_AOT_FUNCTION_TREE_ROOT_INDEX &&
                     entry->function->exportedVariableLength > 0;

    fprintf(file, "static TZrInt64 zr_aot_fn_%u(struct SZrState *state) {\n", (unsigned)entry->flatIndex);
    if (backend_aot_function_uses_vm_shim(entry)) {
        if (entry->flatIndex == ZR_AOT_FUNCTION_TREE_ROOT_INDEX) {
            fprintf(file, "    return ZrLibrary_AotRuntime_InvokeActiveShim(state, ZR_AOT_BACKEND_KIND_C);\n");
        } else {
            fprintf(file, "    return ZrLibrary_AotRuntime_InvokeCurrentClosureShim(state, ZR_AOT_BACKEND_KIND_C);\n");
        }
        fprintf(file, "}\n");
        return;
    }

    fprintf(file, "    ZrAotGeneratedFrame frame;\n");
    fprintf(file, "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_BeginGeneratedFunction(state, %u, &frame));\n",
            (unsigned)entry->flatIndex);

    for (instructionIndex = 0; instructionIndex < entry->function->instructionsLength; instructionIndex++) {
        const TZrInstruction *instruction = &entry->function->instructionsList[instructionIndex];
        TZrUInt32 destinationSlot = instruction->instruction.operandExtra;
        TZrUInt32 operandA1 = instruction->instruction.operand.operand1[0];
        TZrUInt32 operandB1 = instruction->instruction.operand.operand1[1];
        TZrInt32 operandA2 = instruction->instruction.operand.operand2[0];

        fprintf(file, "zr_aot_fn_%u_ins_%u:\n", (unsigned)entry->flatIndex, (unsigned)instructionIndex);
        fprintf(file, "    /* opcode=%u extra=%u op1a=%u op1b=%u op2=%d */\n",
                (unsigned)instruction->instruction.operationCode,
                (unsigned)destinationSlot,
                (unsigned)operandA1,
                (unsigned)operandB1,
                (int)operandA2);

        switch (instruction->instruction.operationCode) {
            case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
                if (backend_aot_constant_requires_materialization(state, entry->function, operandA2)) {
                    fprintf(file,
                            "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CopyConstant(state, &frame, %u, %u));\n",
                            (unsigned)destinationSlot,
                            (unsigned)operandA2);
                } else {
                    backend_aot_write_c_direct_constant_copy(file, destinationSlot, (TZrUInt32)operandA2);
                }
                break;
            case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
                fprintf(file,
                        "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CreateClosure(state, &frame, %u, %u));\n",
                        (unsigned)destinationSlot,
                        (unsigned)operandA1);
                break;
            case ZR_INSTRUCTION_ENUM(GET_STACK):
            case ZR_INSTRUCTION_ENUM(SET_STACK):
                backend_aot_write_c_direct_stack_copy(file, destinationSlot, (TZrUInt32)operandA2);
                break;
            case ZR_INSTRUCTION_ENUM(ADD_INT):
                backend_aot_write_c_direct_add_int(file, destinationSlot, operandA1, operandB1);
                break;
            case ZR_INSTRUCTION_ENUM(GET_MEMBER):
                fprintf(file,
                        "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GetMember(state, &frame, %u, %u, %u));\n",
                        (unsigned)destinationSlot,
                        (unsigned)operandA1,
                        (unsigned)operandB1);
                break;
            case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
                fprintf(file,
                        "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_Call(state, &frame, %u, %u, %u));\n",
                        (unsigned)destinationSlot,
                        (unsigned)operandA1,
                        (unsigned)operandB1);
                break;
            case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
                fprintf(file,
                        "    ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_Call(state, &frame, %u, %u, 0));\n",
                        (unsigned)destinationSlot,
                        (unsigned)operandA1);
                break;
            case ZR_INSTRUCTION_ENUM(TO_INT):
                backend_aot_write_c_direct_to_int(file, destinationSlot, operandA1);
                break;
            case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
                if (publishExports) {
                    fprintf(file,
                            "    return ZrLibrary_AotRuntime_Return(state, &frame, %u, ZR_TRUE);\n",
                            (unsigned)operandA1);
                } else {
                    backend_aot_write_c_direct_return(file, operandA1);
                }
                break;
            default:
                fprintf(file,
                        "    return ZrLibrary_AotRuntime_ReportUnsupportedInstruction(state, %u, %u, %u);\n",
                        (unsigned)entry->flatIndex,
                        (unsigned)instructionIndex,
                        (unsigned)instruction->instruction.operationCode);
                break;
        }
    }

    fprintf(file,
            "    return ZrLibrary_AotRuntime_ReportUnsupportedInstruction(state, %u, %u, 0);\n",
            (unsigned)entry->flatIndex,
            (unsigned)entry->function->instructionsLength);
    fprintf(file, "zr_aot_fail:\n");
    fprintf(file, "    return 0;\n");
    fprintf(file, "}\n");
}

ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotCFileWithOptions(SZrState *state,
                                                               SZrFunction *function,
                                                               const TZrChar *filename,
                                                               const SZrAotWriterOptions *options) {
    SZrAotIrModule module;
    SZrAotCFunctionTable functionTable;
    FILE *file;
    const TZrChar *moduleName;
    const TZrChar *sourceHash;
    const TZrChar *zroHash;
    const TZrChar *inputHash;
    TZrUInt32 inputKind;
    TZrBool hasEmbeddedBlob;
    TZrBool requireExecutableLowering;
    TZrBool success = ZR_FALSE;

    if (state == ZR_NULL || function == ZR_NULL || filename == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&module, 0, sizeof(module));
    memset(&functionTable, 0, sizeof(functionTable));

    if (!backend_aot_build_module(state, function, &module)) {
        return ZR_FALSE;
    }

    if (!backend_aot_build_c_function_table(state, function, &functionTable)) {
        backend_aot_release_module(state, &module);
        return ZR_FALSE;
    }

    file = fopen(filename, "wb");
    if (file == ZR_NULL) {
        backend_aot_release_c_function_table(state, &functionTable);
        backend_aot_release_module(state, &module);
        return ZR_FALSE;
    }

    moduleName = backend_aot_option_text(options, options != ZR_NULL ? options->moduleName : ZR_NULL, "__entry__");
    sourceHash = backend_aot_option_text(options, options != ZR_NULL ? options->sourceHash : ZR_NULL, "unknown");
    zroHash = backend_aot_option_text(options, options != ZR_NULL ? options->zroHash : ZR_NULL, "unknown");
    inputKind = backend_aot_option_input_kind(options);
    inputHash = backend_aot_option_input_hash(options, sourceHash, zroHash);
    hasEmbeddedBlob = options != ZR_NULL && options->embeddedModuleBlob != ZR_NULL && options->embeddedModuleBlobLength > 0;
    requireExecutableLowering = options != ZR_NULL && options->requireExecutableLowering;

    /* Standalone strict lowering rejects unsupported ExecBC. Embedded blobs can fall back to VM shims. */
    if (requireExecutableLowering && !hasEmbeddedBlob) {
        TZrUInt32 functionIndex;

        for (functionIndex = 0; functionIndex < functionTable.count; functionIndex++) {
            const SZrAotCFunctionEntry *entry = &functionTable.entries[functionIndex];
            if (entry->function != ZR_NULL && !backend_aot_function_is_executable_subset(entry->function)) {
                backend_aot_report_first_unsupported_instruction(moduleName, &functionTable);
                fclose(file);
                remove(filename);
                backend_aot_release_c_function_table(state, &functionTable);
                backend_aot_release_module(state, &module);
                return ZR_FALSE;
            }
        }
    }

    fprintf(file, "/* ZR AOT C Backend */\n");
    fprintf(file, "/* SemIR overlay + generated exec thunks. */\n");
    fprintf(file, "/* descriptor.moduleName = %s */\n", moduleName);
    fprintf(file, "/* descriptor.inputKind = %u */\n", (unsigned)inputKind);
    fprintf(file, "/* descriptor.inputHash = %s */\n", inputHash);
    fprintf(file, "/* descriptor.embeddedModuleBlobLength = %llu */\n",
            (unsigned long long)((options != ZR_NULL) ? options->embeddedModuleBlobLength : 0));
    fprintf(file, "#include \"zr_vm_common/zr_aot_abi.h\"\n");
    fprintf(file, "#include \"zr_vm_library/aot_runtime.h\"\n");
    fprintf(file, "\n");
    backend_aot_write_c_guard_macro(file);
    fprintf(file, "\n");
    backend_aot_write_c_contracts(file, module.runtimeContracts);
    fprintf(file, "\n");
    fprintf(file, "/*\n");
    backend_aot_write_instruction_listing(file, " * ", &module);
    fprintf(file, " */\n");
    fprintf(file, "\n");
    backend_aot_write_runtime_contract_array_c(file, module.runtimeContracts);
    fprintf(file, "\n");
    backend_aot_write_embedded_blob_c(file,
                                      options != ZR_NULL ? options->embeddedModuleBlob : ZR_NULL,
                                      options != ZR_NULL ? options->embeddedModuleBlobLength : 0);
    fprintf(file, "\n");
    backend_aot_write_c_function_forward_decls(file, &functionTable);
    fprintf(file, "\n");
    backend_aot_write_c_function_table(file, &functionTable);
    fprintf(file, "\n");
    for (TZrUInt32 functionIndex = 0; functionIndex < functionTable.count; functionIndex++) {
        backend_aot_write_c_function_body(file, state, &functionTable.entries[functionIndex]);
        fprintf(file, "\n");
    }
    fprintf(file, "static const ZrAotCompiledModule zr_aot_module = {\n");
    fprintf(file, "    ZR_VM_AOT_ABI_VERSION,\n");
    fprintf(file, "    ZR_AOT_BACKEND_KIND_C,\n");
    fprintf(file, "    \"%s\",\n", moduleName);
    fprintf(file, "    %u,\n", (unsigned)inputKind);
    fprintf(file, "    \"%s\",\n", inputHash);
    fprintf(file, "    zr_aot_runtime_contracts,\n");
    fprintf(file, "    %s,\n", hasEmbeddedBlob ? "zr_aot_embedded_module_blob" : "ZR_NULL");
    fprintf(file, "    %llu,\n",
            (unsigned long long)((options != ZR_NULL) ? options->embeddedModuleBlobLength : 0));
    fprintf(file, "    zr_aot_function_thunks,\n");
    fprintf(file, "    %u,\n", (unsigned)functionTable.count);
    if (functionTable.count > ZR_AOT_COUNT_NONE) {
        fprintf(file, "    zr_aot_fn_%u,\n", (unsigned)ZR_AOT_FUNCTION_TREE_ROOT_INDEX);
    } else {
        fprintf(file, "    ZR_NULL,\n");
    }
    fprintf(file, "};\n");
    fprintf(file, "\n");
    fprintf(file, "ZR_VM_AOT_EXPORT const ZrAotCompiledModule *ZrVm_GetAotCompiledModule(void) {\n");
    fprintf(file, "    return &zr_aot_module;\n");
    fprintf(file, "}\n");

    fclose(file);
    success = ZR_TRUE;
    backend_aot_release_c_function_table(state, &functionTable);
    backend_aot_release_module(state, &module);
    return success;
}

ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotCFile(SZrState *state,
                                                    SZrFunction *function,
                                                    const TZrChar *filename) {
    return ZrParser_Writer_WriteAotCFileWithOptions(state, function, filename, ZR_NULL);
}

ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotLlvmFileWithOptions(SZrState *state,
                                                                  SZrFunction *function,
                                                                  const TZrChar *filename,
                                                                  const SZrAotWriterOptions *options) {
    SZrAotIrModule module;
    FILE *file;
    const TZrChar *moduleName;
    const TZrChar *sourceHash;
    const TZrChar *zroHash;
    const TZrChar *inputHash;
    TZrUInt32 inputKind;

    if (state == ZR_NULL || function == ZR_NULL || filename == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!backend_aot_build_module(state, function, &module)) {
        return ZR_FALSE;
    }

    file = fopen(filename, "wb");
    if (file == ZR_NULL) {
        backend_aot_release_module(state, &module);
        return ZR_FALSE;
    }

    moduleName = backend_aot_option_text(options, options != ZR_NULL ? options->moduleName : ZR_NULL, "__entry__");
    sourceHash = backend_aot_option_text(options, options != ZR_NULL ? options->sourceHash : ZR_NULL, "unknown");
    zroHash = backend_aot_option_text(options, options != ZR_NULL ? options->zroHash : ZR_NULL, "unknown");
    inputKind = backend_aot_option_input_kind(options);
    inputHash = backend_aot_option_input_hash(options, sourceHash, zroHash);

    fprintf(file, "; ZR AOT LLVM Backend\n");
    fprintf(file, "; SemIR -> AOTIR textual lowering.\n");
    backend_aot_write_llvm_contracts(file, module.runtimeContracts);
    backend_aot_write_runtime_contract_array_llvm(file, module.runtimeContracts);
    fprintf(file, "\n");
    backend_aot_write_instruction_listing(file, "; ", &module);
    fprintf(file, "@zr_aot_module_name = private unnamed_addr constant [%llu x i8] c\"%s\\00\"\n",
            (unsigned long long)(strlen(moduleName) + 1),
            moduleName);
    fprintf(file, "@zr_aot_input_hash = private unnamed_addr constant [%llu x i8] c\"%s\\00\"\n",
            (unsigned long long)(strlen(inputHash) + 1),
            inputHash);
    backend_aot_write_runtime_contract_globals_llvm(file, module.runtimeContracts);
    fprintf(file, "%%ZrAotCompiledModule = type { i32, i32, ptr, i32, ptr, ptr, ptr, i64, ptr, i32, ptr }\n");
    fprintf(file, "define i64 @zr_aot_entry(ptr %%state) {\n");
    fprintf(file, "entry:\n");
    fprintf(file, "  %%ret = call i64 @ZrLibrary_AotRuntime_InvokeActiveShim(ptr %%state, i32 %u)\n",
            (unsigned)ZR_AOT_BACKEND_KIND_LLVM);
    fprintf(file, "  ret i64 %%ret\n");
    fprintf(file, "}\n");
    fprintf(file, "\n");
    fprintf(file, "@zr_aot_module = private constant %%ZrAotCompiledModule {\n");
    fprintf(file, "  i32 %u,\n", (unsigned)ZR_VM_AOT_ABI_VERSION);
    fprintf(file, "  i32 %u,\n", (unsigned)ZR_AOT_BACKEND_KIND_LLVM);
    fprintf(file, "  ptr @zr_aot_module_name,\n");
    fprintf(file, "  i32 %u,\n", (unsigned)inputKind);
    fprintf(file, "  ptr @zr_aot_input_hash,\n");
    fprintf(file, "  ptr @zr_aot_runtime_contracts,\n");
    fprintf(file, "  ptr null,\n");
    fprintf(file, "  i64 0,\n");
    fprintf(file, "  ptr null,\n");
    fprintf(file, "  i32 0,\n");
    fprintf(file, "  ptr @zr_aot_entry\n");
    fprintf(file, "}\n");
    fprintf(file, "\n");
    fprintf(file, "; export-symbol: ZrVm_GetAotCompiledModule\n");
    fprintf(file, "; descriptor.moduleName = %s\n", moduleName);
    fprintf(file, "; descriptor.inputKind = %u\n", (unsigned)inputKind);
    fprintf(file, "; descriptor.inputHash = %s\n", inputHash);
    fprintf(file, "; descriptor.backendKind = llvm\n");
    fprintf(file, "declare i64 @ZrLibrary_AotRuntime_InvokeActiveShim(ptr, i32)\n");
    fprintf(file, "define ptr @ZrVm_GetAotCompiledModule() {\n");
    fprintf(file, "entry_export:\n");
    fprintf(file, "  ret ptr @zr_aot_module\n");
    fprintf(file, "}\n");

    fclose(file);
    backend_aot_release_module(state, &module);
    return ZR_TRUE;
}

ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotLlvmFile(SZrState *state,
                                                       SZrFunction *function,
                                                       const TZrChar *filename) {
    return ZrParser_Writer_WriteAotLlvmFileWithOptions(state, function, filename, ZR_NULL);
}
