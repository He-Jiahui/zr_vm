//
// Minimal SemIR -> AOTIR -> textual backend lowering.
//

#include "zr_vm_parser/writer.h"

#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/memory.h"

#include <stdio.h>
#include <string.h>

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
    TZrUInt32 count = 0;

    if (function == ZR_NULL) {
        return 0;
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
    TZrUInt32 count = 0;

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

ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotCFileWithOptions(SZrState *state,
                                                               SZrFunction *function,
                                                               const TZrChar *filename,
                                                               const SZrAotWriterOptions *options) {
    SZrAotIrModule module;
    FILE *file;
    const TZrChar *moduleName;
    const TZrChar *sourceHash;
    const TZrChar *zroHash;

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

    fprintf(file, "/* ZR AOT C Backend */\n");
    fprintf(file, "/* SemIR -> AOTIR textual lowering. */\n");
    fprintf(file, "#include \"zr_vm_common/zr_aot_abi.h\"\n");
    fprintf(file, "#include \"zr_vm_library/aot_runtime.h\"\n");
    backend_aot_write_c_contracts(file, module.runtimeContracts);
    fprintf(file, "\n");
    backend_aot_write_runtime_contract_array_c(file, module.runtimeContracts);
    fprintf(file, "\n");
    fprintf(file, "static TZrInt64 zr_aot_entry(struct SZrState *state) {\n");
    backend_aot_write_instruction_listing(file, "    // ", &module);
    fprintf(file, "    return ZrLibrary_AotRuntime_InvokeActiveShim(state, ZR_AOT_BACKEND_KIND_C);\n");
    fprintf(file, "}\n");
    fprintf(file, "\n");
    fprintf(file, "static const ZrAotCompiledModuleV1 zr_aot_module = {\n");
    fprintf(file, "    ZR_VM_AOT_ABI_VERSION,\n");
    fprintf(file, "    ZR_AOT_BACKEND_KIND_C,\n");
    fprintf(file, "    \"%s\",\n", moduleName);
    fprintf(file, "    \"%s\",\n", sourceHash);
    fprintf(file, "    \"%s\",\n", zroHash);
    fprintf(file, "    zr_aot_runtime_contracts,\n");
    fprintf(file, "    zr_aot_entry,\n");
    fprintf(file, "};\n");
    fprintf(file, "\n");
    fprintf(file, "ZR_VM_AOT_EXPORT const ZrAotCompiledModuleV1 *ZrVm_GetAotCompiledModule_v1(void) {\n");
    fprintf(file, "    return &zr_aot_module;\n");
    fprintf(file, "}\n");

    fclose(file);
    backend_aot_release_module(state, &module);
    return ZR_TRUE;
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

    fprintf(file, "; ZR AOT LLVM Backend\n");
    fprintf(file, "; SemIR -> AOTIR textual lowering.\n");
    backend_aot_write_llvm_contracts(file, module.runtimeContracts);
    backend_aot_write_runtime_contract_array_llvm(file, module.runtimeContracts);
    fprintf(file, "\n");
    backend_aot_write_instruction_listing(file, "; ", &module);
    fprintf(file, "@zr_aot_module_name = private unnamed_addr constant [%llu x i8] c\"%s\\00\"\n",
            (unsigned long long)(strlen(moduleName) + 1),
            moduleName);
    fprintf(file, "@zr_aot_source_hash = private unnamed_addr constant [%llu x i8] c\"%s\\00\"\n",
            (unsigned long long)(strlen(sourceHash) + 1),
            sourceHash);
    fprintf(file, "@zr_aot_zro_hash = private unnamed_addr constant [%llu x i8] c\"%s\\00\"\n",
            (unsigned long long)(strlen(zroHash) + 1),
            zroHash);
    backend_aot_write_runtime_contract_globals_llvm(file, module.runtimeContracts);
    fprintf(file, "%%ZrAotCompiledModuleV1 = type { i32, i32, ptr, ptr, ptr, ptr, ptr }\n");
    fprintf(file, "define i64 @zr_aot_entry(ptr %%state) {\n");
    fprintf(file, "entry:\n");
    fprintf(file, "  %%ret = call i64 @ZrLibrary_AotRuntime_InvokeActiveShim(ptr %%state, i32 %u)\n",
            (unsigned)ZR_AOT_BACKEND_KIND_LLVM);
    fprintf(file, "  ret i64 %%ret\n");
    fprintf(file, "}\n");
    fprintf(file, "\n");
    fprintf(file, "@zr_aot_module = private constant %%ZrAotCompiledModuleV1 {\n");
    fprintf(file, "  i32 %u,\n", (unsigned)ZR_VM_AOT_ABI_VERSION);
    fprintf(file, "  i32 %u,\n", (unsigned)ZR_AOT_BACKEND_KIND_LLVM);
    fprintf(file, "  ptr @zr_aot_module_name,\n");
    fprintf(file, "  ptr @zr_aot_source_hash,\n");
    fprintf(file, "  ptr @zr_aot_zro_hash,\n");
    fprintf(file, "  ptr @zr_aot_runtime_contracts,\n");
    fprintf(file, "  ptr @zr_aot_entry\n");
    fprintf(file, "}\n");
    fprintf(file, "\n");
    fprintf(file, "; export-symbol: ZrVm_GetAotCompiledModule_v1\n");
    fprintf(file, "; descriptor.moduleName = %s\n", moduleName);
    fprintf(file, "; descriptor.sourceHash = %s\n", sourceHash);
    fprintf(file, "; descriptor.zroHash = %s\n", zroHash);
    fprintf(file, "; descriptor.backendKind = llvm\n");
    fprintf(file, "declare i64 @ZrLibrary_AotRuntime_InvokeActiveShim(ptr, i32)\n");
    fprintf(file, "define ptr @ZrVm_GetAotCompiledModule_v1() {\n");
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
