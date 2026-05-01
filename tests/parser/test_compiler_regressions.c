#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "unity.h"

#include "call_chain_polymorphic_compile_fixture.h"
#include "matrix_add_2d_compile_fixture.h"
#include "runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/task_runtime.h"
#include "zr_vm_core/value.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_ffi/module.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_network/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_library/common_state.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser.h"

typedef struct SZrRegressionTestTimer {
    clock_t startTime;
    clock_t endTime;
} SZrRegressionTestTimer;

typedef struct ZrProjectRunRequest {
    SZrTypeValue *result;
    EZrThreadStatus status;
} ZrProjectRunRequest;

typedef struct ZrMemberCallsiteStats {
    TZrUInt32 cacheEntryCount;
    TZrUInt32 totalPicSlots;
    TZrUInt32 totalHitCount;
    TZrUInt32 totalMissCount;
    const SZrObjectPrototype *receiverPrototypes[16];
    TZrUInt32 uniqueReceiverPrototypeCount;
} ZrMemberCallsiteStats;

static char *read_text_file_owned(const TZrChar *path) {
    FILE *file;
    long fileSize;
    char *buffer;

    if (path == ZR_NULL) {
        return ZR_NULL;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    buffer = (char *)malloc((size_t)fileSize + 1);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }

    if (fileSize > 0 && fread(buffer, 1, (size_t)fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return ZR_NULL;
    }

    buffer[fileSize] = '\0';
    fclose(file);
    return buffer;
}

static void run_project_body(SZrState *state, TZrPtr arguments) {
    ZrProjectRunRequest *request = (ZrProjectRunRequest *)arguments;

    if (state == ZR_NULL || request == ZR_NULL || request->result == ZR_NULL) {
        return;
    }

    request->status = ZrLibrary_Project_Run(state, request->result);
}

static const TZrChar *function_name_or_anonymous(const SZrFunction *function) {
    if (function == ZR_NULL || function->functionName == ZR_NULL) {
        return "<anonymous>";
    }

    return ZrCore_String_GetNativeString(function->functionName);
}

static void member_callsite_stats_note_receiver(ZrMemberCallsiteStats *stats,
                                                const SZrObjectPrototype *receiverPrototype) {
    TZrUInt32 index;

    if (stats == ZR_NULL || receiverPrototype == ZR_NULL) {
        return;
    }

    for (index = 0; index < stats->uniqueReceiverPrototypeCount; index++) {
        if (stats->receiverPrototypes[index] == receiverPrototype) {
            return;
        }
    }

    if (stats->uniqueReceiverPrototypeCount < (TZrUInt32)(sizeof(stats->receiverPrototypes) /
                                                          sizeof(stats->receiverPrototypes[0]))) {
        stats->receiverPrototypes[stats->uniqueReceiverPrototypeCount++] = receiverPrototype;
    }
}

static void collect_member_callsite_stats_recursive(const SZrFunction *function,
                                                    EZrFunctionCallSiteCacheKind kind,
                                                    const char *memberName,
                                                    TZrUInt32 depth,
                                                    ZrMemberCallsiteStats *stats) {
    TZrUInt32 cacheIndex;
    TZrUInt32 childIndex;

    TEST_ASSERT_NOT_NULL(stats);
    TEST_ASSERT_TRUE_MESSAGE(depth < 64, "Callsite cache recursion depth exceeded 64");

    if (function == ZR_NULL || memberName == ZR_NULL) {
        return;
    }

    if (function->callSiteCaches != ZR_NULL) {
        for (cacheIndex = 0; cacheIndex < function->callSiteCacheLength; cacheIndex++) {
            const SZrFunctionCallSiteCacheEntry *entry = &function->callSiteCaches[cacheIndex];
            const char *actualMemberName = ZR_NULL;

            if ((EZrFunctionCallSiteCacheKind)entry->kind != kind ||
                function->memberEntries == ZR_NULL ||
                entry->memberEntryIndex >= function->memberEntryLength ||
                function->memberEntries[entry->memberEntryIndex].symbol == ZR_NULL) {
                continue;
            }

            actualMemberName = ZrCore_String_GetNativeString(function->memberEntries[entry->memberEntryIndex].symbol);
            if (actualMemberName == ZR_NULL || strcmp(actualMemberName, memberName) != 0) {
                continue;
            }

            stats->cacheEntryCount++;
            stats->totalPicSlots += entry->picSlotCount;
            stats->totalHitCount += entry->runtimeHitCount;
            stats->totalMissCount += entry->runtimeMissCount;
            for (TZrUInt32 slotIndex = 0; slotIndex < entry->picSlotCount; slotIndex++) {
                member_callsite_stats_note_receiver(stats, entry->picSlots[slotIndex].cachedReceiverPrototype);
            }
        }
    }

    if (function->childFunctionList == ZR_NULL) {
        return;
    }

    for (childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        collect_member_callsite_stats_recursive(&function->childFunctionList[childIndex],
                                                kind,
                                                memberName,
                                                depth + 1,
                                                stats);
    }
}

static void format_member_callsite_stats(const char *label,
                                         const ZrMemberCallsiteStats *stats,
                                         char *buffer,
                                         size_t bufferSize) {
    int written;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferSize > 0u);

    if (stats == ZR_NULL) {
        buffer[0] = '\0';
        return;
    }

    written = snprintf(buffer,
                       bufferSize,
                       "%s stats: caches=%u picSlots=%u hits=%u misses=%u uniqueReceivers=%u",
                       label != ZR_NULL ? label : "<member>",
                       (unsigned int)stats->cacheEntryCount,
                       (unsigned int)stats->totalPicSlots,
                       (unsigned int)stats->totalHitCount,
                       (unsigned int)stats->totalMissCount,
                       (unsigned int)stats->uniqueReceiverPrototypeCount);
    if (written < 0 || (size_t)written >= bufferSize) {
        buffer[bufferSize - 1] = '\0';
    }
}

static void prepare_project_entry_runtime_module(SZrState *state,
                                                 SZrFunction *function,
                                                 SZrString *sourceName) {
    SZrObjectModule *projectModule;
    TZrUInt64 pathHash;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(sourceName);

    projectModule = ZrCore_Module_Create(state);
    TEST_ASSERT_NOT_NULL(projectModule);

    pathHash = ZrCore_Module_CalculatePathHash(state, sourceName);
    ZrCore_Module_SetInfo(state, projectModule, ZR_NULL, pathHash, sourceName);
    ZrCore_Module_CreatePrototypesFromConstants(state, projectModule, function);
}

#define ZR_TEST_OPCODE_NAME_CASE(INSTRUCTION)                                                                         \
    case ZR_INSTRUCTION_ENUM(INSTRUCTION):                                                                            \
        return #INSTRUCTION;

static const char *instruction_opcode_name(EZrInstructionCode opcode) {
    switch (opcode) {
        ZR_INSTRUCTION_DECLARE(ZR_TEST_OPCODE_NAME_CASE)
        default:
            return "UNKNOWN_OPCODE";
    }
}

static TZrBool find_first_opcode_recursive(const SZrFunction *function,
                                           EZrInstructionCode opcode,
                                           TZrUInt32 depth,
                                           const SZrFunction **outFunction,
                                           TZrUInt32 *outInstructionIndex) {
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE_MESSAGE(depth < 64, "Opcode recursion depth exceeded 64");

    if (outFunction != ZR_NULL) {
        *outFunction = ZR_NULL;
    }
    if (outInstructionIndex != ZR_NULL) {
        *outInstructionIndex = 0;
    }

    for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
        if ((EZrInstructionCode)function->instructionsList[index].instruction.operationCode == opcode) {
            if (outFunction != ZR_NULL) {
                *outFunction = function;
            }
            if (outInstructionIndex != ZR_NULL) {
                *outInstructionIndex = index;
            }
            return ZR_TRUE;
        }
    }

    if (function->childFunctionList != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
            if (find_first_opcode_recursive(&function->childFunctionList[index],
                                            opcode,
                                            depth + 1,
                                            outFunction,
                                            outInstructionIndex)) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static TZrBool find_nth_opcode_recursive(const SZrFunction *function,
                                         EZrInstructionCode opcode,
                                         TZrUInt32 targetOrdinal,
                                         TZrUInt32 depth,
                                         const SZrFunction **outFunction,
                                         TZrUInt32 *outInstructionIndex,
                                         TZrUInt32 *ioSeenCount) {
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(ioSeenCount);
    TEST_ASSERT_TRUE_MESSAGE(depth < 64, "Opcode recursion depth exceeded 64");

    if (outFunction != ZR_NULL) {
        *outFunction = ZR_NULL;
    }
    if (outInstructionIndex != ZR_NULL) {
        *outInstructionIndex = 0;
    }

    for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
        if ((EZrInstructionCode)function->instructionsList[index].instruction.operationCode != opcode) {
            continue;
        }

        (*ioSeenCount)++;
        if (*ioSeenCount != targetOrdinal) {
            continue;
        }

        if (outFunction != ZR_NULL) {
            *outFunction = function;
        }
        if (outInstructionIndex != ZR_NULL) {
            *outInstructionIndex = index;
        }
        return ZR_TRUE;
    }

    if (function->childFunctionList != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
            if (find_nth_opcode_recursive(&function->childFunctionList[index],
                                          opcode,
                                          targetOrdinal,
                                          depth + 1,
                                          outFunction,
                                          outInstructionIndex,
                                          ioSeenCount)) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static const SZrFunctionMemberEntry *find_member_entry_by_symbol(const SZrFunction *function,
                                                                 const char *expectedSymbol) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->memberEntries == ZR_NULL || expectedSymbol == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < function->memberEntryLength; index++) {
        const SZrFunctionMemberEntry *entry = &function->memberEntries[index];
        const char *actualSymbol = entry->symbol != ZR_NULL ? ZrCore_String_GetNativeString(entry->symbol) : ZR_NULL;

        if (actualSymbol != ZR_NULL && strcmp(actualSymbol, expectedSymbol) == 0) {
            return entry;
        }
    }

    return ZR_NULL;
}

static const SZrFunction *find_child_function_by_name_recursive(const SZrFunction *function,
                                                                const char *expectedName,
                                                                TZrUInt32 depth) {
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE_MESSAGE(depth < 64, "Child function recursion depth exceeded 64");

    if (expectedName == ZR_NULL) {
        return ZR_NULL;
    }

    if (function->functionName != ZR_NULL) {
        const char *actualName = ZrCore_String_GetNativeString(function->functionName);
        if (actualName != ZR_NULL && strcmp(actualName, expectedName) == 0) {
            return function;
        }
    }

    if (function->childFunctionList != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
            const SZrFunction *match =
                    find_child_function_by_name_recursive(&function->childFunctionList[index], expectedName, depth + 1);
            if (match != ZR_NULL) {
                return match;
            }
        }
    }

    return ZR_NULL;
}

static void append_instruction_window_line(char *buffer,
                                           size_t bufferSize,
                                           size_t *length,
                                           const SZrFunction *function,
                                           TZrUInt32 instructionIndex) {
    const TZrInstruction *instruction;
    EZrInstructionCode opcode;
    TZrUInt32 lineInSource = 0;
    int written;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_NOT_NULL(length);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE_MESSAGE(instructionIndex < function->instructionsLength,
                             "Instruction window index out of range");

    if (*length >= bufferSize) {
        return;
    }

    instruction = &function->instructionsList[instructionIndex];
    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    if (function->lineInSourceList != ZR_NULL) {
        lineInSource = function->lineInSourceList[instructionIndex];
    }

    written = snprintf(buffer + *length,
                       bufferSize - *length,
                       " #%u %s dst=%u op1=(%u,%u) op2=%d line=%u",
                       (unsigned int)instructionIndex,
                       instruction_opcode_name(opcode),
                       (unsigned int)instruction->instruction.operandExtra,
                       (unsigned int)instruction->instruction.operand.operand1[0],
                       (unsigned int)instruction->instruction.operand.operand1[1],
                       (int)instruction->instruction.operand.operand2[0],
                       (unsigned int)lineInSource);
    if (written <= 0) {
        return;
    }

    if ((size_t)written >= bufferSize - *length) {
        *length = bufferSize;
        return;
    }

    *length += (size_t)written;
}

static void append_slot_metadata(char *buffer,
                                 size_t bufferSize,
                                 size_t *length,
                                 const SZrFunction *function,
                                 TZrUInt32 instructionIndex,
                                 TZrUInt32 slot) {
    TZrBool wroteHeader = ZR_FALSE;

    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_NOT_NULL(length);
    TEST_ASSERT_NOT_NULL(function);

    if (*length >= bufferSize) {
        return;
    }

    if (function->localVariableList != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->localVariableLength; index++) {
            const SZrFunctionLocalVariable *local = &function->localVariableList[index];
            const char *name = local->name != ZR_NULL ? ZrCore_String_GetNativeString(local->name) : "<unnamed>";
            int written;

            if (local->stackSlot != slot) {
                continue;
            }

            if (!wroteHeader) {
                written = snprintf(buffer + *length,
                                   bufferSize - *length,
                                   " slot%u{",
                                   (unsigned int)slot);
                if (written <= 0 || (size_t)written >= bufferSize - *length) {
                    *length = bufferSize;
                    return;
                }
                *length += (size_t)written;
                wroteHeader = ZR_TRUE;
            }

            written = snprintf(buffer + *length,
                               bufferSize - *length,
                               " local=%s[%u,%u)%s",
                               name != ZR_NULL ? name : "<unnamed>",
                               (unsigned int)local->offsetActivate,
                               (unsigned int)local->offsetDead,
                               ((TZrUInt32)local->offsetActivate <= instructionIndex &&
                                instructionIndex < (TZrUInt32)local->offsetDead)
                                       ? "*"
                                       : "");
            if (written <= 0 || (size_t)written >= bufferSize - *length) {
                *length = bufferSize;
                return;
            }
            *length += (size_t)written;
        }
    }

    if (function->typedLocalBindings != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->typedLocalBindingLength; index++) {
            const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];
            const char *name = binding->name != ZR_NULL ? ZrCore_String_GetNativeString(binding->name) : "<unnamed>";
            int written;

            if (binding->stackSlot != slot) {
                continue;
            }

            if (!wroteHeader) {
                written = snprintf(buffer + *length,
                                   bufferSize - *length,
                                   " slot%u{",
                                   (unsigned int)slot);
                if (written <= 0 || (size_t)written >= bufferSize - *length) {
                    *length = bufferSize;
                    return;
                }
                *length += (size_t)written;
                wroteHeader = ZR_TRUE;
            }

            written = snprintf(buffer + *length,
                               bufferSize - *length,
                               " binding=%s",
                               name != ZR_NULL ? name : "<unnamed>");
            if (written <= 0 || (size_t)written >= bufferSize - *length) {
                *length = bufferSize;
                return;
            }
            *length += (size_t)written;
        }
    }

    if (function->instructionsList != ZR_NULL) {
        TZrUInt32 writerCount = 0;
        for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
            const TZrInstruction *instruction = &function->instructionsList[index];
            int written;

            if (instruction->instruction.operandExtra != slot) {
                continue;
            }

            if (!wroteHeader) {
                written = snprintf(buffer + *length,
                                   bufferSize - *length,
                                   " slot%u{",
                                   (unsigned int)slot);
                if (written <= 0 || (size_t)written >= bufferSize - *length) {
                    *length = bufferSize;
                    return;
                }
                *length += (size_t)written;
                wroteHeader = ZR_TRUE;
            }

            if (writerCount < 8) {
                EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
                written = snprintf(buffer + *length,
                                   bufferSize - *length,
                                   " writer#%u=%s",
                                   (unsigned int)index,
                                   instruction_opcode_name(opcode));
                if (written <= 0 || (size_t)written >= bufferSize - *length) {
                    *length = bufferSize;
                    return;
                }
                *length += (size_t)written;

                if ((opcode == ZR_INSTRUCTION_ENUM(GETUPVAL) || opcode == ZR_INSTRUCTION_ENUM(GET_CLOSURE)) &&
                    function->closureValueList != ZR_NULL &&
                    instruction->instruction.operand.operand1[0] < function->closureValueLength) {
                    const SZrFunctionClosureVariable *closure =
                            &function->closureValueList[instruction->instruction.operand.operand1[0]];
                    const char *closureName =
                            closure->name != ZR_NULL ? ZrCore_String_GetNativeString(closure->name) : "<unnamed>";
                    written = snprintf(buffer + *length,
                                       bufferSize - *length,
                                       "(closure=%s inStack=%u index=%u)",
                                       closureName != ZR_NULL ? closureName : "<unnamed>",
                                       (unsigned int)(closure->inStack ? 1u : 0u),
                                       (unsigned int)closure->index);
                    if (written <= 0 || (size_t)written >= bufferSize - *length) {
                        *length = bufferSize;
                        return;
                    }
                    *length += (size_t)written;
                }
            }
            writerCount++;
        }

        if (wroteHeader) {
            int written = snprintf(buffer + *length,
                                   bufferSize - *length,
                                   " writers=%u}",
                                   (unsigned int)writerCount);
            if (written <= 0 || (size_t)written >= bufferSize - *length) {
                *length = bufferSize;
                return;
            }
            *length += (size_t)written;
        }
    }
}

static void build_opcode_window_message(const SZrFunction *rootFunction,
                                        EZrInstructionCode opcode,
                                        char *buffer,
                                        size_t bufferSize) {
    const SZrFunction *ownerFunction = ZR_NULL;
    TZrUInt32 instructionIndex = 0;
    TZrUInt32 firstIndex;
    TZrUInt32 endIndex;
    size_t length = 0;
    int written;

    TEST_ASSERT_NOT_NULL(rootFunction);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE_MESSAGE(bufferSize > 0, "Opcode window buffer size must be positive");

    buffer[0] = '\0';
    if (!find_first_opcode_recursive(rootFunction, opcode, 0, &ownerFunction, &instructionIndex) ||
        ownerFunction == ZR_NULL) {
        snprintf(buffer, bufferSize, "No %s instruction found", instruction_opcode_name(opcode));
        return;
    }

    firstIndex = instructionIndex > 10 ? instructionIndex - 10 : 0;
    endIndex = instructionIndex + 10;
    if (endIndex >= ownerFunction->instructionsLength) {
        endIndex = ownerFunction->instructionsLength - 1;
    }

    written = snprintf(buffer,
                       bufferSize,
                       "First %s remains in function '%s' at instruction %u",
                       instruction_opcode_name(opcode),
                       function_name_or_anonymous(ownerFunction),
                       (unsigned int)instructionIndex);
    if (written <= 0 || (size_t)written >= bufferSize) {
        return;
    }
    length = (size_t)written;

    for (TZrUInt32 index = firstIndex; index <= endIndex; index++) {
        append_instruction_window_line(buffer, bufferSize, &length, ownerFunction, index);
    }

    if (opcode == ZR_INSTRUCTION_ENUM(GET_BY_INDEX) || opcode == ZR_INSTRUCTION_ENUM(SET_BY_INDEX)) {
        const TZrInstruction *instruction = &ownerFunction->instructionsList[instructionIndex];
        append_slot_metadata(buffer,
                             bufferSize,
                             &length,
                             ownerFunction,
                             instructionIndex,
                             instruction->instruction.operand.operand1[0]);
        append_slot_metadata(buffer,
                             bufferSize,
                             &length,
                             ownerFunction,
                             instructionIndex,
                             instruction->instruction.operand.operand1[1]);
    }
}

static void build_nth_opcode_window_message(const SZrFunction *rootFunction,
                                            EZrInstructionCode opcode,
                                            TZrUInt32 occurrence,
                                            char *buffer,
                                            size_t bufferSize) {
    const SZrFunction *ownerFunction = ZR_NULL;
    TZrUInt32 instructionIndex = 0;
    TZrUInt32 seenCount = 0;
    TZrUInt32 firstIndex;
    TZrUInt32 endIndex;
    size_t length = 0;
    int written;

    TEST_ASSERT_NOT_NULL(rootFunction);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE_MESSAGE(bufferSize > 0, "Nth opcode window buffer size must be positive");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0u, occurrence, "Opcode occurrence must be positive");

    buffer[0] = '\0';
    if (!find_nth_opcode_recursive(rootFunction,
                                   opcode,
                                   occurrence,
                                   0,
                                   &ownerFunction,
                                   &instructionIndex,
                                   &seenCount) ||
        ownerFunction == ZR_NULL) {
        snprintf(buffer,
                 bufferSize,
                 "No %s instruction found at occurrence %u",
                 instruction_opcode_name(opcode),
                 (unsigned int)occurrence);
        return;
    }

    firstIndex = instructionIndex > 10 ? instructionIndex - 10 : 0;
    endIndex = instructionIndex + 10;
    if (endIndex >= ownerFunction->instructionsLength) {
        endIndex = ownerFunction->instructionsLength - 1;
    }

    written = snprintf(buffer,
                       bufferSize,
                       "%uth %s remains in function '%s' at instruction %u",
                       (unsigned int)occurrence,
                       instruction_opcode_name(opcode),
                       function_name_or_anonymous(ownerFunction),
                       (unsigned int)instructionIndex);
    if (written <= 0 || (size_t)written >= bufferSize) {
        return;
    }
    length = (size_t)written;

    for (TZrUInt32 index = firstIndex; index <= endIndex; index++) {
        append_instruction_window_line(buffer, bufferSize, &length, ownerFunction, index);
    }
}

static TZrBool opcode_is_super_array_get_int_family(EZrInstructionCode opcode) {
    return opcode == ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT) ||
           opcode == ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST) ||
           opcode == ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS) ||
           opcode == ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST);
}

static TZrBool opcode_is_super_array_set_int_family(EZrInstructionCode opcode) {
    return opcode == ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT) ||
           opcode == ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT_ITEMS);
}

static void assert_super_array_int_ops_do_not_reload_adjacent_temp_slots(const SZrFunction *function, TZrUInt32 depth) {
    char message[256];

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE_MESSAGE(depth < 64, "Super-array adjacency recursion depth exceeded 64");

    for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
        const TZrInstruction *instruction = &function->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        TZrUInt16 readSlots[3];
        TZrUInt32 readCount = 0;

        if (opcode_is_super_array_get_int_family(opcode)) {
            readSlots[readCount++] = instruction->instruction.operand.operand1[0];
            readSlots[readCount++] = instruction->instruction.operand.operand1[1];
        } else if (opcode_is_super_array_set_int_family(opcode)) {
            readSlots[readCount++] = instruction->instruction.operandExtra;
            readSlots[readCount++] = instruction->instruction.operand.operand1[0];
            readSlots[readCount++] = instruction->instruction.operand.operand1[1];
        } else {
            continue;
        }

        for (TZrUInt32 back = 1; back <= 3 && back <= index; back++) {
            const TZrInstruction *previous = &function->instructionsList[index - back];

            if ((EZrInstructionCode)previous->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK)) {
                continue;
            }

            for (TZrUInt32 readIndex = 0; readIndex < readCount; readIndex++) {
                snprintf(message,
                         sizeof(message),
                         "Function '%s' still reloads temp slot %u immediately before %s at instruction %u",
                         function_name_or_anonymous(function),
                         (unsigned int)previous->instruction.operandExtra,
                         opcode_is_super_array_get_int_family(opcode) ? "SUPER_ARRAY_GET_INT family"
                                                                       : "SUPER_ARRAY_SET_INT family",
                         (unsigned int)index);
                TEST_ASSERT_NOT_EQUAL_MESSAGE((int)previous->instruction.operandExtra,
                                              (int)readSlots[readIndex],
                                              message);
            }
        }
    }

    if (function->childFunctionList != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
            assert_super_array_int_ops_do_not_reload_adjacent_temp_slots(&function->childFunctionList[index], depth + 1);
        }
    }
}

static TZrUInt32 count_opcode_recursive(const SZrFunction *function, EZrInstructionCode opcode, TZrUInt32 depth) {
    TZrUInt32 count = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE_MESSAGE(depth < 64, "Opcode recursion depth exceeded 64");

    for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
        if ((EZrInstructionCode)function->instructionsList[index].instruction.operationCode == opcode) {
            count++;
        }
    }

    if (function->childFunctionList != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
            count += count_opcode_recursive(&function->childFunctionList[index], opcode, depth + 1);
        }
    }

    return count;
}

static TZrUInt32 count_tail_call_family_recursive(const SZrFunction *function, TZrUInt32 depth) {
    return count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(META_TAIL_CALL), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED), depth);
}

static TZrUInt32 count_non_tail_call_family_recursive(const SZrFunction *function, TZrUInt32 depth) {
    return count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(FUNCTION_CALL), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(DYN_CALL), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(META_CALL), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED), depth);
}

static void append_opcode_hits_recursive(const SZrFunction *function,
                                         EZrInstructionCode opcode,
                                         TZrUInt32 depth,
                                         char *buffer,
                                         size_t bufferSize,
                                         size_t *length,
                                         TZrUInt32 *ioHitCount) {
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_NOT_NULL(length);
    TEST_ASSERT_NOT_NULL(ioHitCount);
    TEST_ASSERT_TRUE_MESSAGE(depth < 64, "Opcode hit recursion depth exceeded 64");

    if (function->instructionsList != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
            const TZrInstruction *instruction = &function->instructionsList[index];
            EZrInstructionCode instructionOpcode =
                    (EZrInstructionCode)instruction->instruction.operationCode;
            TZrUInt32 lineInSource = 0;
            int written;

            if (instructionOpcode != opcode) {
                continue;
            }

            if (function->lineInSourceList != ZR_NULL) {
                lineInSource = function->lineInSourceList[index];
            }

            (*ioHitCount)++;
            if (*ioHitCount > 8 || *length >= bufferSize) {
                continue;
            }

            written = snprintf(buffer + *length,
                               bufferSize - *length,
                               " [%u]%s#%u(line=%u,dst=%u",
                               (unsigned int)*ioHitCount,
                               function_name_or_anonymous(function),
                               (unsigned int)index,
                               (unsigned int)lineInSource,
                               (unsigned int)instruction->instruction.operandExtra);
            if (written <= 0) {
                continue;
            }

            if ((size_t)written >= bufferSize - *length) {
                *length = bufferSize;
                return;
            }

            *length += (size_t)written;

            switch (instructionOpcode) {
                case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
                case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
                case ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL):
                case ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL):
                case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL):
                case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL):
                case ZR_INSTRUCTION_ENUM(DYN_CALL):
                case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
                case ZR_INSTRUCTION_ENUM(META_CALL):
                case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
                    written = snprintf(buffer + *length,
                                       bufferSize - *length,
                                       ",callee=%u,args=%u",
                                       (unsigned int)instruction->instruction.operand.operand1[0],
                                       (unsigned int)instruction->instruction.operand.operand1[1]);
                    break;
                case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):
                case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
                case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS):
                case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS):
                case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS):
                case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS):
                case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS):
                case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS):
                case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS):
                case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS):
                    written = snprintf(buffer + *length,
                                       bufferSize - *length,
                                       ",callee=%u,args=0",
                                       (unsigned int)instruction->instruction.operand.operand1[0]);
                    break;
                default:
                    written = snprintf(buffer + *length, bufferSize - *length, ")");
                    break;
            }
            if (written <= 0) {
                continue;
            }

            if ((size_t)written >= bufferSize - *length) {
                *length = bufferSize;
                return;
            }

            *length += (size_t)written;

            if (instructionOpcode == ZR_INSTRUCTION_ENUM(FUNCTION_CALL) ||
                instructionOpcode == ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL) ||
                instructionOpcode == ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL) ||
                instructionOpcode == ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL) ||
                instructionOpcode == ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL) ||
                instructionOpcode == ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL) ||
                instructionOpcode == ZR_INSTRUCTION_ENUM(DYN_CALL) ||
                instructionOpcode == ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL) ||
                instructionOpcode == ZR_INSTRUCTION_ENUM(META_CALL) ||
                instructionOpcode == ZR_INSTRUCTION_ENUM(META_TAIL_CALL) ||
                instructionOpcode == ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS) ||
                instructionOpcode == ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS) ||
                instructionOpcode == ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS) ||
                instructionOpcode == ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS) ||
                instructionOpcode == ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS) ||
                instructionOpcode == ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS) ||
                instructionOpcode == ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS) ||
                instructionOpcode == ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS) ||
                instructionOpcode == ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS) ||
                instructionOpcode == ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS)) {
                written = snprintf(buffer + *length, bufferSize - *length, ")");
                if (written <= 0) {
                    continue;
                }

                if ((size_t)written >= bufferSize - *length) {
                    *length = bufferSize;
                    return;
                }

                *length += (size_t)written;
            }
        }
    }

    if (function->childFunctionList != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
            append_opcode_hits_recursive(&function->childFunctionList[index],
                                         opcode,
                                         depth + 1,
                                         buffer,
                                         bufferSize,
                                         length,
                                         ioHitCount);
        }
    }
}

static void build_opcode_hits_message(const SZrFunction *rootFunction,
                                      EZrInstructionCode opcode,
                                      char *buffer,
                                      size_t bufferSize) {
    size_t length = 0;
    TZrUInt32 hitCount = 0;

    TEST_ASSERT_NOT_NULL(rootFunction);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE_MESSAGE(bufferSize > 0, "Opcode hit buffer size must be positive");

    buffer[0] = '\0';
    append_opcode_hits_recursive(rootFunction, opcode, 0, buffer, bufferSize, &length, &hitCount);

    if (hitCount == 0) {
        snprintf(buffer, bufferSize, "No %s hits", instruction_opcode_name(opcode));
        return;
    }

    if (hitCount > 8 && length < bufferSize) {
        snprintf(buffer + length,
                 bufferSize - length,
                 " ... total=%u",
                 (unsigned int)hitCount);
    }
}

static TZrUInt32 count_opcode_on_source_line_recursive(const SZrFunction *function,
                                                       EZrInstructionCode opcode,
                                                       TZrUInt32 lineNumber,
                                                       TZrUInt32 depth) {
    TZrUInt32 count = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE_MESSAGE(depth < 64, "Source-line opcode recursion depth exceeded 64");

    if (function->instructionsList != ZR_NULL && function->lineInSourceList != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
            if ((EZrInstructionCode)function->instructionsList[index].instruction.operationCode == opcode &&
                function->lineInSourceList[index] == lineNumber) {
                count++;
            }
        }
    }

    if (function->childFunctionList != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
            count += count_opcode_on_source_line_recursive(&function->childFunctionList[index],
                                                           opcode,
                                                           lineNumber,
                                                           depth + 1);
        }
    }

    return count;
}

static TZrBool find_first_opcode_on_source_line_recursive(const SZrFunction *function,
                                                          EZrInstructionCode opcode,
                                                          TZrUInt32 lineNumber,
                                                          TZrUInt32 depth,
                                                          const SZrFunction **outFunction,
                                                          TZrUInt32 *outInstructionIndex) {
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE_MESSAGE(depth < 64, "Source-line opcode recursion depth exceeded 64");

    if (outFunction != ZR_NULL) {
        *outFunction = ZR_NULL;
    }
    if (outInstructionIndex != ZR_NULL) {
        *outInstructionIndex = 0;
    }

    if (function->instructionsList != ZR_NULL && function->lineInSourceList != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
            if ((EZrInstructionCode)function->instructionsList[index].instruction.operationCode == opcode &&
                function->lineInSourceList[index] == lineNumber) {
                if (outFunction != ZR_NULL) {
                    *outFunction = function;
                }
                if (outInstructionIndex != ZR_NULL) {
                    *outInstructionIndex = index;
                }
                return ZR_TRUE;
            }
        }
    }

    if (function->childFunctionList != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
            if (find_first_opcode_on_source_line_recursive(&function->childFunctionList[index],
                                                           opcode,
                                                           lineNumber,
                                                           depth + 1,
                                                           outFunction,
                                                           outInstructionIndex)) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static void build_opcode_window_message_for_source_line(const SZrFunction *rootFunction,
                                                        EZrInstructionCode opcode,
                                                        TZrUInt32 lineNumber,
                                                        char *buffer,
                                                        size_t bufferSize) {
    const SZrFunction *ownerFunction = ZR_NULL;
    TZrUInt32 instructionIndex = 0;
    TZrUInt32 firstIndex;
    TZrUInt32 endIndex;
    size_t length = 0;
    int written;

    TEST_ASSERT_NOT_NULL(rootFunction);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE_MESSAGE(bufferSize > 0, "Opcode window buffer size must be positive");

    buffer[0] = '\0';
    if (!find_first_opcode_on_source_line_recursive(rootFunction,
                                                    opcode,
                                                    lineNumber,
                                                    0,
                                                    &ownerFunction,
                                                    &instructionIndex) ||
        ownerFunction == ZR_NULL) {
        snprintf(buffer,
                 bufferSize,
                 "No %s instruction found on source line %u",
                 instruction_opcode_name(opcode),
                 (unsigned int)lineNumber);
        return;
    }

    firstIndex = instructionIndex > 10 ? instructionIndex - 10 : 0;
    endIndex = instructionIndex + 10;
    if (endIndex >= ownerFunction->instructionsLength) {
        endIndex = ownerFunction->instructionsLength - 1;
    }

    written = snprintf(buffer,
                       bufferSize,
                       "First %s on source line %u remains in function '%s' at instruction %u",
                       instruction_opcode_name(opcode),
                       (unsigned int)lineNumber,
                       function_name_or_anonymous(ownerFunction),
                       (unsigned int)instructionIndex);
    if (written <= 0 || (size_t)written >= bufferSize) {
        return;
    }
    length = (size_t)written;

    for (TZrUInt32 index = firstIndex; index <= endIndex; index++) {
        append_instruction_window_line(buffer, bufferSize, &length, ownerFunction, index);
    }
}

static const SZrFunctionLocalVariable *find_local_variable_by_name(const SZrFunction *function, const char *name) {
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(name);

    if (function->localVariableList == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->localVariableLength; index++) {
        const SZrFunctionLocalVariable *local = &function->localVariableList[index];
        const char *localName = local->name != ZR_NULL ? ZrCore_String_GetNativeString(local->name) : ZR_NULL;

        if (localName != ZR_NULL && strcmp(localName, name) == 0) {
            return local;
        }
    }

    return ZR_NULL;
}

static const SZrFunctionExportedVariable *find_exported_variable_by_name(const SZrFunction *function, const char *name) {
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(name);

    if (function->exportedVariables == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->exportedVariableLength; index++) {
        const SZrFunctionExportedVariable *exported = &function->exportedVariables[index];
        const char *exportedName =
                exported->name != ZR_NULL ? ZrCore_String_GetNativeString(exported->name) : ZR_NULL;

        if (exportedName != ZR_NULL && strcmp(exportedName, name) == 0) {
            return exported;
        }
    }

    return ZR_NULL;
}

static TZrUInt32 find_first_execution_location_offset_for_line(const SZrFunction *function, TZrUInt32 lineNumber) {
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(function->executionLocationInfoList);

    for (TZrUInt32 index = 0; index < function->executionLocationInfoLength; index++) {
        if (function->executionLocationInfoList[index].lineInSource == lineNumber) {
            return (TZrUInt32)function->executionLocationInfoList[index].currentInstructionOffset;
        }
    }

    return UINT32_MAX;
}

static TZrBool opcode_is_signed_add_const_family(EZrInstructionCode opcode) {
    return opcode == ZR_INSTRUCTION_ENUM(ADD_INT_CONST) ||
           opcode == ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST) ||
           opcode == ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST) ||
           opcode == ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST);
}

static TZrBool opcode_is_signed_sub_right_constant_pair_opcode(EZrInstructionCode opcode) {
    return opcode == ZR_INSTRUCTION_ENUM(SUB_INT) || opcode == ZR_INSTRUCTION_ENUM(SUB_SIGNED);
}

static TZrUInt32 count_stack_self_update_int_const_triplets_recursive(const SZrFunction *function,
                                                                      TZrUInt32 depth) {
    TZrUInt32 count = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE_MESSAGE(depth < 64, "Stack self-update recursion depth exceeded 64");

    if (function->instructionsLength >= 3) {
        for (TZrUInt32 index = 0; index + 2 < function->instructionsLength; index++) {
            const TZrInstruction *loadInstruction = &function->instructionsList[index];
            const TZrInstruction *arithmeticInstruction = &function->instructionsList[index + 1];
            const TZrInstruction *storeInstruction = &function->instructionsList[index + 2];
            EZrInstructionCode arithmeticOpcode =
                    (EZrInstructionCode)arithmeticInstruction->instruction.operationCode;

            if ((EZrInstructionCode)loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) ||
                !opcode_is_signed_add_const_family(arithmeticOpcode) ||
                (EZrInstructionCode)storeInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) {
                continue;
            }

            if ((TZrUInt32)arithmeticInstruction->instruction.operand.operand1[0] !=
                    loadInstruction->instruction.operandExtra ||
                (TZrUInt32)storeInstruction->instruction.operand.operand2[0] !=
                    arithmeticInstruction->instruction.operandExtra ||
                storeInstruction->instruction.operandExtra !=
                    (TZrUInt32)loadInstruction->instruction.operand.operand2[0]) {
                continue;
            }

            count++;
        }
    }

    if (function->childFunctionList != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
            count += count_stack_self_update_int_const_triplets_recursive(&function->childFunctionList[index],
                                                                          depth + 1);
        }
    }

    return count;
}

static TZrBool opcode_is_direct_result_store_fold_candidate(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(ADD_INT):
        case ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_INT):
        case ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrUInt32 count_direct_result_store_followed_by_set_stack_pairs_in_function_range(
        const SZrFunction *function,
        TZrUInt32 startIndex) {
    TZrUInt32 count = 0;

    TEST_ASSERT_NOT_NULL(function);

    if (startIndex >= function->instructionsLength || function->instructionsLength < 2) {
        return 0;
    }

    for (TZrUInt32 index = startIndex; index + 1 < function->instructionsLength; index++) {
        const TZrInstruction *producerInstruction = &function->instructionsList[index];
        const TZrInstruction *storeInstruction = &function->instructionsList[index + 1];
        EZrInstructionCode producerOpcode =
                (EZrInstructionCode)producerInstruction->instruction.operationCode;

        if (!opcode_is_direct_result_store_fold_candidate(producerOpcode) ||
            (EZrInstructionCode)storeInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) {
            continue;
        }

        if ((TZrUInt32)storeInstruction->instruction.operand.operand2[0] != producerInstruction->instruction.operandExtra) {
            continue;
        }

        count++;
    }

    return count;
}

static TZrUInt32 count_known_vm_member_call_result_store_pairs_in_function_range(
        const SZrFunction *function,
        TZrUInt32 startIndex) {
    TZrUInt32 count = 0;

    TEST_ASSERT_NOT_NULL(function);

    if (startIndex >= function->instructionsLength || function->instructionsLength < 2) {
        return 0;
    }

    for (TZrUInt32 index = startIndex; index + 1 < function->instructionsLength; index++) {
        const TZrInstruction *callInstruction = &function->instructionsList[index];
        const TZrInstruction *storeInstruction = &function->instructionsList[index + 1];

        if ((EZrInstructionCode)callInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL) ||
            (EZrInstructionCode)storeInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) {
            continue;
        }

        if ((TZrUInt32)storeInstruction->instruction.operand.operand2[0] != callInstruction->instruction.operandExtra) {
            continue;
        }

        count++;
    }

    return count;
}

static TZrUInt32 count_known_vm_member_call_receiver_copy_triplets_in_function_range(
        const SZrFunction *function,
        TZrUInt32 startIndex) {
    TZrUInt32 count = 0;

    TEST_ASSERT_NOT_NULL(function);

    if (startIndex >= function->instructionsLength || function->instructionsLength < 3) {
        return 0;
    }

    for (TZrUInt32 index = startIndex; index + 2 < function->instructionsLength; index++) {
        const TZrInstruction *loadInstruction = &function->instructionsList[index];
        const TZrInstruction *copyInstruction = &function->instructionsList[index + 1];
        const TZrInstruction *callInstruction = &function->instructionsList[index + 2];

        if ((EZrInstructionCode)loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) ||
            (EZrInstructionCode)copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK) ||
            (EZrInstructionCode)callInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL)) {
            continue;
        }

        if ((TZrUInt32)copyInstruction->instruction.operand.operand2[0] != loadInstruction->instruction.operandExtra ||
            callInstruction->instruction.operandExtra != loadInstruction->instruction.operandExtra) {
            continue;
        }

        count++;
    }

    return count;
}

static TZrUInt32 count_sub_int_right_constant_pairs_recursive(const SZrFunction *function, TZrUInt32 depth) {
    TZrUInt32 count = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE_MESSAGE(depth < 64, "SUB_INT right-constant recursion depth exceeded 64");

    if (function->instructionsLength >= 2) {
        for (TZrUInt32 index = 1; index < function->instructionsLength; index++) {
            const TZrInstruction *constantInstruction = &function->instructionsList[index - 1];
            const TZrInstruction *subInstruction = &function->instructionsList[index];

            if ((EZrInstructionCode)constantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
                !opcode_is_signed_sub_right_constant_pair_opcode(
                        (EZrInstructionCode)subInstruction->instruction.operationCode)) {
                continue;
            }

            if (subInstruction->instruction.operand.operand1[1] != constantInstruction->instruction.operandExtra) {
                continue;
            }

            count++;
        }
    }

    if (function->childFunctionList != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
            count += count_sub_int_right_constant_pairs_recursive(&function->childFunctionList[index], depth + 1);
        }
    }

    return count;
}

static TZrBool test_instruction_supports_get_stack_copy_forward_rewrite(const TZrInstruction *instruction,
                                                                        TZrUInt32 oldSlot,
                                                                        TZrBool *outReadsSlot) {
    EZrInstructionCode opcode;
    TZrBool readsSlot = ZR_FALSE;

    TEST_ASSERT_NOT_NULL(instruction);
    TEST_ASSERT_NOT_NULL(outReadsSlot);

    *outReadsSlot = ZR_FALSE;
    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(NOP):
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
        case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
        case ZR_INSTRUCTION_ENUM(GETUPVAL):
        case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
        case ZR_INSTRUCTION_ENUM(GET_GLOBAL):
        case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
        case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(GET_STACK):
        case ZR_INSTRUCTION_ENUM(SET_STACK):
            readsSlot = (TZrUInt32)instruction->instruction.operand.operand2[0] == oldSlot;
            break;
        case ZR_INSTRUCTION_ENUM(JUMP_IF):
            readsSlot = instruction->instruction.operandExtra == oldSlot;
            break;
        case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
            readsSlot = instruction->instruction.operandExtra == oldSlot ||
                        instruction->instruction.operand.operand1[0] == oldSlot;
            break;
        case ZR_INSTRUCTION_ENUM(TO_BOOL):
        case ZR_INSTRUCTION_ENUM(TO_INT):
        case ZR_INSTRUCTION_ENUM(TO_UINT):
        case ZR_INSTRUCTION_ENUM(TO_FLOAT):
        case ZR_INSTRUCTION_ENUM(TO_STRING):
        case ZR_INSTRUCTION_ENUM(TO_STRUCT):
        case ZR_INSTRUCTION_ENUM(TO_OBJECT):
        case ZR_INSTRUCTION_ENUM(NEG):
        case ZR_INSTRUCTION_ENUM(TYPEOF):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(GET_MEMBER):
        case ZR_INSTRUCTION_ENUM(SET_MEMBER):
            readsSlot = instruction->instruction.operand.operand1[0] == oldSlot;
            break;
        case ZR_INSTRUCTION_ENUM(ADD):
        case ZR_INSTRUCTION_ENUM(ADD_INT):
        case ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
        case ZR_INSTRUCTION_ENUM(ADD_STRING):
        case ZR_INSTRUCTION_ENUM(SUB):
        case ZR_INSTRUCTION_ENUM(SUB_INT):
        case ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
        case ZR_INSTRUCTION_ENUM(MUL):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
        case ZR_INSTRUCTION_ENUM(DIV):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
        case ZR_INSTRUCTION_ENUM(MOD):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_STRING):
        case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
        case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(BITWISE_AND):
        case ZR_INSTRUCTION_ENUM(BITWISE_OR):
        case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT):
        case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT):
            readsSlot = instruction->instruction.operand.operand1[0] == oldSlot ||
                        instruction->instruction.operand.operand1[1] == oldSlot;
            break;
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_BIND_ITEMS):
            readsSlot = (TZrUInt32)instruction->instruction.operand.operand2[0] == oldSlot;
            break;
        case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT_ITEMS):
            readsSlot = instruction->instruction.operandExtra == oldSlot ||
                        instruction->instruction.operand.operand1[0] == oldSlot ||
                        instruction->instruction.operand.operand1[1] == oldSlot;
            break;
        default:
            if (instruction->instruction.operandExtra == oldSlot ||
                instruction->instruction.operand.operand1[0] == oldSlot ||
                instruction->instruction.operand.operand1[1] == oldSlot ||
                (TZrUInt32)instruction->instruction.operand.operand2[0] == oldSlot) {
                return ZR_FALSE;
            }
            return ZR_TRUE;
    }

    *outReadsSlot = readsSlot;
    return ZR_TRUE;
}

static TZrBool test_instruction_writes_slot(const TZrInstruction *instruction, TZrUInt32 slot) {
    EZrInstructionCode opcode;

    TEST_ASSERT_NOT_NULL(instruction);

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(GET_STACK):
        case ZR_INSTRUCTION_ENUM(SET_STACK):
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
        case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
        case ZR_INSTRUCTION_ENUM(SET_CLOSURE):
        case ZR_INSTRUCTION_ENUM(GETUPVAL):
        case ZR_INSTRUCTION_ENUM(SETUPVAL):
        case ZR_INSTRUCTION_ENUM(GET_GLOBAL):
        case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
        case ZR_INSTRUCTION_ENUM(GET_MEMBER):
        case ZR_INSTRUCTION_ENUM(SET_MEMBER):
        case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_BIND_ITEMS):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT_ITEMS):
        case ZR_INSTRUCTION_ENUM(TO_BOOL):
        case ZR_INSTRUCTION_ENUM(TO_INT):
        case ZR_INSTRUCTION_ENUM(TO_UINT):
        case ZR_INSTRUCTION_ENUM(TO_FLOAT):
        case ZR_INSTRUCTION_ENUM(TO_STRING):
        case ZR_INSTRUCTION_ENUM(TO_STRUCT):
        case ZR_INSTRUCTION_ENUM(TO_OBJECT):
        case ZR_INSTRUCTION_ENUM(ADD):
        case ZR_INSTRUCTION_ENUM(ADD_INT):
        case ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
        case ZR_INSTRUCTION_ENUM(ADD_STRING):
        case ZR_INSTRUCTION_ENUM(SUB):
        case ZR_INSTRUCTION_ENUM(SUB_INT):
        case ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
        case ZR_INSTRUCTION_ENUM(MUL):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
        case ZR_INSTRUCTION_ENUM(NEG):
        case ZR_INSTRUCTION_ENUM(DIV):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
        case ZR_INSTRUCTION_ENUM(MOD):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_STRING):
        case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
        case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(BITWISE_AND):
        case ZR_INSTRUCTION_ENUM(BITWISE_OR):
        case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT):
        case ZR_INSTRUCTION_ENUM(ITER_INIT):
        case ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT):
        case ZR_INSTRUCTION_ENUM(ITER_CURRENT):
        case ZR_INSTRUCTION_ENUM(TYPEOF):
            return instruction->instruction.operandExtra == slot;
        default:
            return ZR_FALSE;
    }
}

static TZrUInt32 count_forwardable_get_stack_copy_reads_in_function_range(const SZrFunction *function,
                                                                          TZrUInt32 startIndex) {
    TZrUInt32 count = 0;
    TZrUInt32 index;
    TZrUInt32 blockStart = 0;

    TEST_ASSERT_NOT_NULL(function);

    if (function->instructionsList == ZR_NULL || function->instructionsLength < 2 || startIndex >= function->instructionsLength) {
        return 0;
    }

    for (index = startIndex; index + 1 < function->instructionsLength; index++) {
        const TZrInstruction *copyInstruction = &function->instructionsList[index];
        TZrUInt32 sourceSlot;
        TZrUInt32 tempSlot;
        TZrUInt32 scan;
        TZrBool replacedAny = ZR_FALSE;
        TZrBool supported = ZR_TRUE;
        TZrBool tempValueDiesByCurrentRewrite = ZR_FALSE;
        TZrBool stoppedBeforeCurrent = ZR_FALSE;

        if ((EZrInstructionCode)copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK)) {
            continue;
        }

        sourceSlot = (TZrUInt32)copyInstruction->instruction.operand.operand2[0];
        tempSlot = copyInstruction->instruction.operandExtra;
        if (sourceSlot == tempSlot) {
            continue;
        }

        if (index == 0 || (EZrInstructionCode)function->instructionsList[index - 1].instruction.operationCode == ZR_INSTRUCTION_ENUM(JUMP) ||
            (EZrInstructionCode)function->instructionsList[index - 1].instruction.operationCode == ZR_INSTRUCTION_ENUM(JUMP_IF) ||
            (EZrInstructionCode)function->instructionsList[index - 1].instruction.operationCode ==
                    ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED)) {
            blockStart = index;
        }

        for (scan = index + 1; scan < function->instructionsLength; scan++) {
            const TZrInstruction *instruction = &function->instructionsList[scan];
            TZrBool readsTemp = ZR_FALSE;
            TZrBool writesSource;
            TZrBool writesTemp;

            if (scan > blockStart &&
                ((EZrInstructionCode)function->instructionsList[scan - 1].instruction.operationCode == ZR_INSTRUCTION_ENUM(JUMP) ||
                 (EZrInstructionCode)function->instructionsList[scan - 1].instruction.operationCode ==
                         ZR_INSTRUCTION_ENUM(JUMP_IF) ||
                 (EZrInstructionCode)function->instructionsList[scan - 1].instruction.operationCode ==
                         ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED))) {
                break;
            }

            supported = test_instruction_supports_get_stack_copy_forward_rewrite(instruction, tempSlot, &readsTemp);
            if (!supported) {
                break;
            }

            if (readsTemp) {
                replacedAny = ZR_TRUE;
            }

            writesSource = test_instruction_writes_slot(instruction, sourceSlot);
            writesTemp = test_instruction_writes_slot(instruction, tempSlot);
            if (writesSource || writesTemp) {
                if (readsTemp) {
                    tempValueDiesByCurrentRewrite = writesTemp;
                } else {
                    stoppedBeforeCurrent = ZR_TRUE;
                }
                break;
            }
        }

        if (replacedAny && (!stoppedBeforeCurrent || tempValueDiesByCurrentRewrite)) {
            count++;
        }
    }

    return count;
}

static TZrUInt32 count_less_equal_signed_jump_if_pairs_in_function_range(const SZrFunction *function,
                                                                         TZrUInt32 startIndex) {
    TZrUInt32 count = 0;

    TEST_ASSERT_NOT_NULL(function);

    if (function->instructionsList == ZR_NULL || function->instructionsLength < 2 || startIndex >= function->instructionsLength) {
        return 0;
    }

    for (TZrUInt32 index = startIndex; index + 1 < function->instructionsLength; index++) {
        const TZrInstruction *compareInstruction = &function->instructionsList[index];
        const TZrInstruction *jumpIfInstruction = &function->instructionsList[index + 1];

        if ((EZrInstructionCode)compareInstruction->instruction.operationCode !=
                    ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED) ||
            (EZrInstructionCode)jumpIfInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF)) {
            continue;
        }

        if (jumpIfInstruction->instruction.operandExtra != compareInstruction->instruction.operandExtra) {
            continue;
        }

        count++;
    }

    return count;
}

static TZrUInt32 count_jump_if_greater_signed_in_function_range(const SZrFunction *function, TZrUInt32 startIndex) {
    TZrUInt32 count = 0;

    TEST_ASSERT_NOT_NULL(function);

    if (function->instructionsList == ZR_NULL || startIndex >= function->instructionsLength) {
        return 0;
    }

    for (TZrUInt32 index = startIndex; index < function->instructionsLength; index++) {
        if ((EZrInstructionCode)function->instructionsList[index].instruction.operationCode ==
            ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED)) {
            count++;
        }
    }

    return count;
}

static TZrInt32 find_top_level_string_constant_index(const SZrFunction *function, const char *text) {
    if (function == ZR_NULL || text == ZR_NULL || function->constantValueList == ZR_NULL) {
        return -1;
    }

    for (TZrUInt32 index = 0; index < function->constantValueLength; index++) {
        const SZrTypeValue *constant = &function->constantValueList[index];
        const char *constantText;

        if (constant->type != ZR_VALUE_TYPE_STRING || constant->value.object == ZR_NULL) {
            continue;
        }

        constantText = ZrCore_String_GetNativeString(ZR_CAST(SZrString *, constant->value.object));
        if (constantText != ZR_NULL && strcmp(constantText, text) == 0) {
            return (TZrInt32)index;
        }
    }

    return -1;
}

static TZrInt32 find_top_level_int_constant_index(const SZrFunction *function, TZrInt64 value) {
    if (function == ZR_NULL || function->constantValueList == ZR_NULL) {
        return -1;
    }

    for (TZrUInt32 index = 0; index < function->constantValueLength; index++) {
        const SZrTypeValue *constant = &function->constantValueList[index];

        if (constant->type == ZR_VALUE_TYPE_INT64 && constant->value.nativeObject.nativeInt64 == value) {
            return (TZrInt32)index;
        }
    }

    return -1;
}

static TZrUInt32 count_get_constant_uses_recursive(const SZrFunction *function,
                                                   TZrUInt32 constantIndex,
                                                   TZrUInt32 depth) {
    TZrUInt32 count = 0;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE_MESSAGE(depth < 64, "Constant use recursion depth exceeded 64");

    for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
        const TZrInstruction *instruction = &function->instructionsList[index];

        if ((EZrInstructionCode)instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(GET_CONSTANT) &&
            (TZrUInt32)instruction->instruction.operand.operand2[0] == constantIndex) {
            count++;
        }
    }

    if (function->childFunctionList != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
            count += count_get_constant_uses_recursive(&function->childFunctionList[index], constantIndex, depth + 1);
        }
    }

    return count;
}

static TZrUInt32 count_get_constant_set_stack_pairs_for_constant_in_function_range(const SZrFunction *function,
                                                                                   TZrUInt32 startIndex,
                                                                                   TZrUInt32 constantIndex) {
    TZrUInt32 count = 0;

    TEST_ASSERT_NOT_NULL(function);

    if (function->instructionsList == ZR_NULL || startIndex >= function->instructionsLength ||
        function->instructionsLength < 2) {
        return 0;
    }

    for (TZrUInt32 index = startIndex; index + 1 < function->instructionsLength; index++) {
        const TZrInstruction *constantInstruction = &function->instructionsList[index];
        const TZrInstruction *storeInstruction = &function->instructionsList[index + 1];

        if ((EZrInstructionCode)constantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
            (TZrUInt32)constantInstruction->instruction.operand.operand2[0] != constantIndex ||
            (EZrInstructionCode)storeInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) {
            continue;
        }

        if ((TZrUInt32)storeInstruction->instruction.operand.operand2[0] != constantInstruction->instruction.operandExtra) {
            continue;
        }

        count++;
    }

    return count;
}

static TZrBool function_tree_contains_exact_child_pointer(const SZrFunction *function, const SZrFunction *target) {
    if (function == ZR_NULL || target == ZR_NULL || function->childFunctionList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
        const SZrFunction *child = &function->childFunctionList[index];
        if (child == target || function_tree_contains_exact_child_pointer(child, target)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void assert_function_constant_operands_in_range_recursive(SZrState *state,
                                                                 const SZrFunction *function,
                                                                 TZrUInt32 depth) {
    char message[256];

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE_MESSAGE(depth < 64, "Function constant recursion depth exceeded 64");

    for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
        const TZrInstruction *instruction = &function->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;

        if (opcode == ZR_INSTRUCTION_ENUM(GET_CONSTANT) || opcode == ZR_INSTRUCTION_ENUM(SET_CONSTANT)) {
            TZrInt32 constantIndex = (TZrInt32)instruction->instruction.operand.operand2[0];

            snprintf(message,
                     sizeof(message),
                     "Function '%s' emitted negative constant index %d at instruction %u",
                     function_name_or_anonymous(function),
                     (int)constantIndex,
                     (unsigned int)index);
            TEST_ASSERT_TRUE_MESSAGE(constantIndex >= 0, message);

            snprintf(message,
                     sizeof(message),
                     "Function '%s' emitted constant index %u but pool length is %u at instruction %u",
                     function_name_or_anonymous(function),
                     (unsigned int)constantIndex,
                     (unsigned int)function->constantValueLength,
                     (unsigned int)index);
            TEST_ASSERT_TRUE_MESSAGE((TZrUInt32)constantIndex < function->constantValueLength, message);
        }
    }

    for (TZrUInt32 index = 0; index < function->constantValueLength; index++) {
        const SZrTypeValue *constant = &function->constantValueList[index];

        if (constant->type == ZR_VALUE_TYPE_FUNCTION && constant->value.object != ZR_NULL) {
            const SZrFunction *child = ZR_CAST_FUNCTION(state, constant->value.object);
            assert_function_constant_operands_in_range_recursive(state, child, depth + 1);
        }
    }

    if (function->childFunctionList != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
            const SZrFunction *child = &function->childFunctionList[index];
            assert_function_constant_operands_in_range_recursive(state, child, depth + 1);
        }
    }
}

static void assert_create_closure_targets_are_reachable_children_recursive(SZrState *state,
                                                                           const SZrFunction *function,
                                                                           TZrUInt32 depth) {
    char message[256];

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE_MESSAGE(depth < 64, "CREATE_CLOSURE child reachability recursion depth exceeded 64");

    for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
        const TZrInstruction *instruction = &function->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;

        if (opcode == ZR_INSTRUCTION_ENUM(CREATE_CLOSURE)) {
            TZrUInt32 constantIndex = instruction->instruction.operand.operand1[0];
            const SZrTypeValue *constant;
            const SZrFunction *targetFunction = ZR_NULL;

            snprintf(message,
                     sizeof(message),
                     "Function '%s' emitted CREATE_CLOSURE with constant index %u but pool length is %u at instruction %u",
                     function_name_or_anonymous(function),
                     (unsigned int)constantIndex,
                     (unsigned int)function->constantValueLength,
                     (unsigned int)index);
            TEST_ASSERT_TRUE_MESSAGE(constantIndex < function->constantValueLength, message);

            constant = &function->constantValueList[constantIndex];
            if ((constant->type == ZR_VALUE_TYPE_FUNCTION || constant->type == ZR_VALUE_TYPE_CLOSURE) &&
                constant->value.object != ZR_NULL &&
                constant->value.object->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
                targetFunction = ZR_CAST_FUNCTION(state, constant->value.object);
            }

            snprintf(message,
                     sizeof(message),
                     "Function '%s' emitted CREATE_CLOSURE with non-function constant at instruction %u",
                     function_name_or_anonymous(function),
                     (unsigned int)index);
            TEST_ASSERT_NOT_NULL_MESSAGE(targetFunction, message);

            snprintf(message,
                     sizeof(message),
                     "Function '%s' emitted CREATE_CLOSURE for function constant %u that is not reachable from childFunctions at instruction %u",
                     function_name_or_anonymous(function),
                     (unsigned int)constantIndex,
                     (unsigned int)index);
            TEST_ASSERT_TRUE_MESSAGE(function_tree_contains_exact_child_pointer(function, targetFunction), message);
        }
    }

    if (function->childFunctionList != ZR_NULL) {
        for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
            assert_create_closure_targets_are_reachable_children_recursive(state,
                                                                           &function->childFunctionList[index],
                                                                           depth + 1);
        }
    }
}

void test_class_member_nested_functions_keep_constant_indices_in_range(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Class Member Nested Functions Keep Constant Indices In Range";
    SZrState *state;
    char sourcePath[512];
    char *source = ZR_NULL;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Nested function constant cache reset",
                 "Testing that class member methods compile with local constant indices after nested function state resets");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    snprintf(sourcePath,
             sizeof(sourcePath),
             "%s/fixtures/projects/classes/src/main.zr",
             ZR_VM_TESTS_SOURCE_DIR);
    source = read_text_file_owned(sourcePath);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_CreateFromNative(state, "projects_classes_main.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    assert_function_constant_operands_in_range_recursive(state, function, 0);

    ZrCore_Function_Free(state, function);
    free(source);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_lambda_create_closure_targets_are_reachable_from_child_function_graph(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Lambda Create Closure Targets Are Reachable From Child Function Graph";
    const char *source =
            "var build = () => {\n"
            "    var emit = () => { return 1; };\n"
            "    return emit();\n"
            "};\n"
            "return build();";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Lambda child function graph completeness",
                 "Testing that CREATE_CLOSURE function constants are reachable from childFunctions instead of relying on constant-only recovery");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    sourceName = ZrCore_String_CreateFromNative(state, "lambda_child_function_graph_regression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    assert_create_closure_targets_are_reachable_children_recursive(state, function, 0);

    ZrCore_Function_Free(state, function);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_classes_full_module_compiles_without_static_and_receiver_signature_regressions(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Classes Full Module Compiles Without Static And Receiver Signature Regressions";
    SZrState *state;
    char sourcePath[512];
    char *source = ZR_NULL;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Static class access and instance parameter metadata",
                 "Testing that classes_full hero module compiles with static member access and instance method signatures");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    snprintf(sourcePath,
             sizeof(sourcePath),
             "%s/fixtures/projects/classes_full/src/hero.zr",
             ZR_VM_TESTS_SOURCE_DIR);
    source = read_text_file_owned(sourcePath);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_CreateFromNative(state, "projects_classes_full_hero.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    ZrCore_Function_Free(state, function);
    free(source);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_native_network_optional_argument_import_compiles_without_unknown_parameter_blowup(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Native Network Optional Argument Import Compiles Without Unknown Parameter Blowup";
    const char *source =
            "var network = %import(\"zr.network\");\n"
            "network.tcp.connect(\"127.0.0.1\", 1);\n";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Imported native optional parameters",
                 "Testing that optional-argument native members like zr.network.tcp.connect do not treat UNKNOWN parameter counts as allocation sizes during compile");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    TEST_ASSERT_TRUE(ZrVmLibNetwork_Register(state->global));

    sourceName = ZrCore_String_CreateFromNative(state, "native_network_optional_parameter_regression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    ZrCore_Function_Free(state, function);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_reserved_type_query_targets_compile_without_explicit_imports(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Reserved %type Targets Compile Without Explicit Imports";
    const char *source =
            "markClass(target: %type Class): void { return; }\n"
            "markStruct(target: %type Struct): void { return; }\n"
            "markFunction(target: %type Function): void { return; }\n"
            "markField(target: %type Field): void { return; }\n"
            "markMethod(target: %type Method): void { return; }\n"
            "markProperty(target: %type Property): void { return; }\n"
            "markParameter(target: %type Parameter): void { return; }\n"
            "markObject(target: %type Object): void { return; }\n"
            "return 0;";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Reserved reflection target pseudo-types",
                 "Testing that %type Class/Struct/Function/Field/Method/Property/Parameter/Object remain valid reserved targets without reopening imported type globals");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    sourceName = ZrCore_String_CreateFromNative(state, "reserved_type_query_targets_regression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    ZrCore_Function_Free(state, function);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_qualified_container_types_compile_through_function_predeclaration_paths(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Qualified Container Types Compile Through Function Predeclaration Paths";
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "forward(value: container.Array<int>): container.Array<int> {\n"
            "    var local: container.Array<int> = value;\n"
            "    return local;\n"
            "}\n"
            "host(): int {\n"
            "    var scoped = %import(\"zr.container\");\n"
            "    nested(value: scoped.Array<int>): scoped.Array<int> {\n"
            "        var local: scoped.Array<int> = value;\n"
            "        return local;\n"
            "    }\n"
            "    var xs: scoped.Array<int> = new scoped.Array<int>();\n"
            "    return nested(xs).length;\n"
            "}\n"
            "return host();";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Qualified container types through predeclaration",
                 "Testing that module-qualified container types remain available when function signatures are predeclared before the normal statement compilation pass");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    TEST_ASSERT_TRUE(ZrVmLibContainer_Register(state->global));

    sourceName = ZrCore_String_CreateFromNative(state, "qualified_container_predeclare_regression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    ZrCore_Function_Free(state, function);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_native_network_loopback_runtime_returns_expected_payload(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Native Network Loopback Runtime Returns Expected Payload";
    const char *source =
            "var network = %import(\"zr.network\");\n"
            "var tcp = network.tcp;\n"
            "var udp = network.udp;\n"
            "var listener = tcp.listen(\"127.0.0.1\", 0);\n"
            "var client = tcp.connect(\"127.0.0.1\", listener.port());\n"
            "var server = listener.accept(3000);\n"
            "var ping = \"ping\";\n"
            "var pong = \"pong\";\n"
            "var echo = \"echo\";\n"
            "var wrotePing = client.write(ping);\n"
            "var readPing = server.read(16, 3000);\n"
            "var wrotePong = server.write(pong);\n"
            "var readPong = client.read(16, 3000);\n"
            "var socket = udp.bind(\"127.0.0.1\", 0);\n"
            "var sentEcho = socket.send(\"127.0.0.1\", socket.port(), echo);\n"
            "var packet = socket.receive(16, 3000);\n"
            "server.close();\n"
            "client.close();\n"
            "listener.close();\n"
            "socket.close();\n"
            "if (wrotePing != 4 || wrotePong != 4 || sentEcho != 4) {\n"
            "    return \"NETWORK_LOOPBACK_FAIL write\";\n"
            "}\n"
            "if (readPing != ping || readPong != pong) {\n"
            "    return \"NETWORK_LOOPBACK_FAIL tcp\";\n"
            "}\n"
            "if (packet == null || packet.payload != echo || packet.length != 4) {\n"
            "    return \"NETWORK_LOOPBACK_FAIL udp\";\n"
            "}\n"
            "return \"NETWORK_LOOPBACK_PASS \" + readPing + \" \" + readPong + \" \" + packet.payload;\n";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;
    SZrTypeValue result;
    SZrString *resultString;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Loopback TCP/UDP runtime",
                 "Testing that zr.network TCP and UDP loopback client/server flows return the expected payload on the current platform");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    TEST_ASSERT_TRUE(ZrVmLibNetwork_Register(state->global));

    sourceName = ZrCore_String_CreateFromNative(state, "native_network_loopback_runtime_regression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.type);
    resultString = ZR_CAST_STRING(state, result.value.object);
    TEST_ASSERT_NOT_NULL(resultString);
    TEST_ASSERT_EQUAL_STRING("NETWORK_LOOPBACK_PASS ping pong echo", ZrCore_String_GetNativeString(resultString));

    ZrCore_Function_Free(state, function);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_native_network_loopback_project_run_returns_expected_payload(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Native Network Loopback Project Run Returns Expected Payload";
    char projectPath[512];
    SZrGlobalState *global = ZR_NULL;
    SZrState *state = ZR_NULL;
    ZrProjectRunRequest request;
    EZrThreadStatus outerStatus;
    SZrTypeValue result;
    SZrString *resultString;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Project runtime loopback",
                 "Testing that ZrLibrary_Project_Run executes the network_loopback project without escaping through an unhandled runtime exception");

    snprintf(projectPath,
             sizeof(projectPath),
             "%s/fixtures/projects/network_loopback/network_loopback.zrp",
             ZR_VM_TESTS_SOURCE_DIR);

    global = ZrLibrary_CommonState_CommonGlobalState_New(projectPath);
    TEST_ASSERT_NOT_NULL(global);

    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);

    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE(ZrVmLibNetwork_Register(global));

    request.result = &result;
    ZrCore_Value_ResetAsNull(request.result);
    request.status = ZR_THREAD_STATUS_INVALID;
    outerStatus = ZrCore_Exception_TryRun(state, run_project_body, &request);

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, outerStatus);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, request.status);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, request.result->type);

    resultString = ZR_CAST_STRING(state, request.result->value.object);
    TEST_ASSERT_NOT_NULL(resultString);
    TEST_ASSERT_EQUAL_STRING("NETWORK_LOOPBACK_PASS ping pong echo", ZrCore_String_GetNativeString(resultString));

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    ZR_TEST_DIVIDER();
}

void test_imported_source_module_type_stubs_do_not_serialize_into_entry_prototype_data(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Imported Source Module Type Stubs Stay Out Of Entry Prototype Data";
    char projectPath[512];
    char sourcePath[512];
    char *source = ZR_NULL;
    SZrGlobalState *global = ZR_NULL;
    SZrState *state = ZR_NULL;
    SZrString *sourceName;
    SZrFunction *function;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Compiler prototype serialization",
                 "Testing that importing a source module for type analysis does not serialize the imported module's runtime type stubs into the current entry function");

    snprintf(projectPath,
             sizeof(projectPath),
             "%s/fixtures/projects/network_loopback/network_loopback.zrp",
             ZR_VM_TESTS_SOURCE_DIR);
    snprintf(sourcePath,
             sizeof(sourcePath),
             "%s/fixtures/projects/network_loopback/src/main.zr",
             ZR_VM_TESTS_SOURCE_DIR);

    global = ZrLibrary_CommonState_CommonGlobalState_New(projectPath);
    TEST_ASSERT_NOT_NULL(global);

    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE(ZrVmLibNetwork_Register(global));

    source = read_text_file_owned(sourcePath);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_CreateFromNative(state, sourcePath);
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(0, function->prototypeCount);
    TEST_ASSERT_TRUE(function->prototypeData == ZR_NULL || function->prototypeDataLength == 0);

    free(source);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    ZR_TEST_DIVIDER();
}

void test_project_local_struct_pair_shadows_native_pair_at_runtime(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Project Local Struct Pair Shadows Native Pair At Runtime";
    char projectPath[512];
    SZrGlobalState *global = ZR_NULL;
    SZrState *state = ZR_NULL;
    ZrProjectRunRequest request;
    EZrThreadStatus outerStatus;
    SZrTypeValue result;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Project type shadowing",
                 "Testing that a project-local struct Pair keeps its constructor and field access even when another module imports zr.container's native Pair");

    snprintf(projectPath,
             sizeof(projectPath),
             "%s/fixtures/projects/project_pair_shadowing/project_pair_shadowing.zrp",
             ZR_VM_TESTS_SOURCE_DIR);

    global = ZrLibrary_CommonState_CommonGlobalState_New(projectPath);
    TEST_ASSERT_NOT_NULL(global);

    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);

    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE(ZrVmLibMath_Register(global));
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(global));
    TEST_ASSERT_TRUE(ZrVmLibContainer_Register(global));
    TEST_ASSERT_TRUE(ZrVmLibFfi_Register(global));
    TEST_ASSERT_TRUE(ZrVmLibNetwork_Register(global));

    request.result = &result;
    ZrCore_Value_ResetAsNull(request.result);
    request.status = ZR_THREAD_STATUS_INVALID;
    outerStatus = ZrCore_Exception_TryRun(state, run_project_body, &request);

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, outerStatus);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, request.status);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, request.result->type);
    TEST_ASSERT_EQUAL_INT64(3, request.result->value.nativeObject.nativeInt64);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    ZR_TEST_DIVIDER();
}

void test_lsp_language_feature_matrix_runtime_returns_expected_total(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "LSP Language Feature Matrix Runtime Returns Expected Total";
    char projectPath[512];
    SZrGlobalState *global = ZR_NULL;
    SZrState *state = ZR_NULL;
    ZrProjectRunRequest request;
    EZrThreadStatus outerStatus;
    SZrTypeValue result;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Project runtime type resolution",
                 "Testing that the language-feature matrix resolves bare in-module types against the current module before sibling or native modules with the same type name");

    snprintf(projectPath,
             sizeof(projectPath),
             "%s/fixtures/projects/lsp_language_feature_matrix/lsp_language_feature_matrix.zrp",
             ZR_VM_TESTS_SOURCE_DIR);

    global = ZrLibrary_CommonState_CommonGlobalState_New(projectPath);
    TEST_ASSERT_NOT_NULL(global);

    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);

    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE(ZrVmLibMath_Register(global));
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(global));
    TEST_ASSERT_TRUE(ZrVmLibContainer_Register(global));
    TEST_ASSERT_TRUE(ZrVmLibFfi_Register(global));
    TEST_ASSERT_TRUE(ZrVmLibNetwork_Register(global));
    TEST_ASSERT_TRUE(ZrCore_TaskRuntime_RegisterBuiltins(global));

    request.result = &result;
    ZrCore_Value_ResetAsNull(request.result);
    request.status = ZR_THREAD_STATUS_INVALID;
    outerStatus = ZrCore_Exception_TryRun(state, run_project_body, &request);

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, outerStatus);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, request.status);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, request.result->type);
    TEST_ASSERT_EQUAL_INT64(64, request.result->value.nativeObject.nativeInt64);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    ZR_TEST_DIVIDER();
}

void test_lsp_language_feature_matrix_copy_runtime_keeps_top_level_closure_captures_stable(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "LSP Language Feature Matrix Copy Runtime Keeps Top Level Closure Captures Stable";
    char projectPath[512];
    SZrGlobalState *global = ZR_NULL;
    SZrState *state = ZR_NULL;
    ZrProjectRunRequest request;
    EZrThreadStatus outerStatus;
    SZrTypeValue result;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Top-level declaration closure capture stability",
                 "Testing that declaration-ready exported closures keep imported-module captures stable even when the entry body uses lower stack slots during initializer calls");

    snprintf(projectPath,
             sizeof(projectPath),
             "%s/fixtures/projects/lsp_language_feature_matrix_copy/lsp_language_feature_matrix.zrp",
             ZR_VM_TESTS_SOURCE_DIR);

    global = ZrLibrary_CommonState_CommonGlobalState_New(projectPath);
    TEST_ASSERT_NOT_NULL(global);

    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);

    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE(ZrVmLibMath_Register(global));
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(global));
    TEST_ASSERT_TRUE(ZrVmLibContainer_Register(global));
    TEST_ASSERT_TRUE(ZrVmLibFfi_Register(global));
    TEST_ASSERT_TRUE(ZrVmLibNetwork_Register(global));
    TEST_ASSERT_TRUE(ZrCore_TaskRuntime_RegisterBuiltins(global));

    request.result = &result;
    ZrCore_Value_ResetAsNull(request.result);
    request.status = ZR_THREAD_STATUS_INVALID;
    outerStatus = ZrCore_Exception_TryRun(state, run_project_body, &request);

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, outerStatus);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, request.status);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, request.result->type);
    TEST_ASSERT_EQUAL_INT64(57, request.result->value.nativeObject.nativeInt64);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    ZR_TEST_DIVIDER();
}

void test_decorator_import_project_run_returns_expected_total(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Decorator Import Project Run Returns Expected Total";
    char projectPath[512];
    SZrGlobalState *global = ZR_NULL;
    SZrState *state = ZR_NULL;
    ZrProjectRunRequest request;
    EZrThreadStatus outerStatus;
    SZrTypeValue result;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Decorator import project runtime",
                 "Testing that the decorator_import project executes through ZrLibrary_Project_Run and preserves decorator metadata plus imported module call results without crashing.");

    snprintf(projectPath,
             sizeof(projectPath),
             "%s/fixtures/projects/decorator_import/decorator_import.zrp",
             ZR_VM_TESTS_SOURCE_DIR);

    global = ZrLibrary_CommonState_CommonGlobalState_New(projectPath);
    TEST_ASSERT_NOT_NULL(global);

    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);

    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE(ZrVmLibMath_Register(global));
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(global));
    TEST_ASSERT_TRUE(ZrVmLibContainer_Register(global));
    TEST_ASSERT_TRUE(ZrVmLibFfi_Register(global));
    TEST_ASSERT_TRUE(ZrVmLibNetwork_Register(global));
    TEST_ASSERT_TRUE(ZrCore_TaskRuntime_RegisterBuiltins(global));

    request.result = &result;
    ZrCore_Value_ResetAsNull(request.result);
    request.status = ZR_THREAD_STATUS_INVALID;
    outerStatus = ZrCore_Exception_TryRun(state, run_project_body, &request);

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, outerStatus);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, request.status);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, request.result->type);
    TEST_ASSERT_EQUAL_INT64(31, request.result->value.nativeObject.nativeInt64);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    ZR_TEST_DIVIDER();
}

void test_language_debug_gauntlet_project_run_returns_expected_banner_and_checksum(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Language Debug Gauntlet Project Run Returns Expected Banner And Checksum";
    char projectPath[512];
    SZrGlobalState *global = ZR_NULL;
    SZrState *state = ZR_NULL;
    ZrProjectRunRequest request;
    EZrThreadStatus outerStatus;
    SZrTypeValue result;
    SZrString *resultString;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Project runtime gauntlet",
                 "Testing that the multi-module gauntlet project keeps compile-time fixed-array sizing, import chaining, branch validation, and deterministic output stable under the normal project run path.");

    snprintf(projectPath,
             sizeof(projectPath),
             "%s/fixtures/projects/language_debug_gauntlet/language_debug_gauntlet.zrp",
             ZR_VM_TESTS_SOURCE_DIR);

    global = ZrLibrary_CommonState_CommonGlobalState_New(projectPath);
    TEST_ASSERT_NOT_NULL(global);

    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);

    ZrParser_ToGlobalState_Register(state);

    request.result = &result;
    ZrCore_Value_ResetAsNull(request.result);
    request.status = ZR_THREAD_STATUS_INVALID;
    outerStatus = ZrCore_Exception_TryRun(state, run_project_body, &request);

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, outerStatus);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, request.status);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, request.result->type);

    resultString = ZR_CAST_STRING(state, request.result->value.object);
    TEST_ASSERT_NOT_NULL(resultString);
    TEST_ASSERT_EQUAL_STRING("GAUNTLET_OK checksum=13910", ZrCore_String_GetNativeString(resultString));

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    ZR_TEST_DIVIDER();
}

void test_benchmark_numeric_loops_project_run_returns_expected_checksum(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Benchmark Numeric Loops Project Run Returns Expected Checksum";
    char projectPath[512];
    SZrGlobalState *global = ZR_NULL;
    SZrState *state = ZR_NULL;
    ZrProjectRunRequest request;
    EZrThreadStatus outerStatus;
    SZrTypeValue result;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Benchmark project import compilation",
                 "Testing that a benchmark project importing local bench_config compiles and runs without tripping module-init summary validation");

    snprintf(projectPath,
             sizeof(projectPath),
             "%s/benchmarks/cases/numeric_loops/zr/benchmark_numeric_loops.zrp",
             ZR_VM_TESTS_SOURCE_DIR);

    global = ZrLibrary_CommonState_CommonGlobalState_New(projectPath);
    TEST_ASSERT_NOT_NULL(global);

    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);

    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(global));

    request.result = &result;
    ZrCore_Value_ResetAsNull(request.result);
    request.status = ZR_THREAD_STATUS_INVALID;
    outerStatus = ZrCore_Exception_TryRun(state, run_project_body, &request);

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, outerStatus);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, request.status);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, request.result->type);
    TEST_ASSERT_EQUAL_INT64(793446923, request.result->value.nativeObject.nativeInt64);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    ZR_TEST_DIVIDER();
}

void test_dispatch_loops_benchmark_project_runtime_keeps_step_member_pic_coverage(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Dispatch Loops Benchmark Runtime Keeps Step Member PIC Coverage";
    char projectPath[512];
    char sourcePath[512];
    char *source = ZR_NULL;
    SZrGlobalState *global = ZR_NULL;
    SZrState *state = ZR_NULL;
    SZrString *sourceName = ZR_NULL;
    SZrFunction *function = ZR_NULL;
    ZrMemberCallsiteStats stepStats;
    char stepStatsMessage[256];
    SZrTypeValue result;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("dispatch benchmark member PIC coverage",
                 "Testing that the real dispatch_loops benchmark run keeps `step` member callsites warm across all four worker receiver variants instead of thrashing a smaller PIC and falling back repeatedly.");

    snprintf(projectPath,
             sizeof(projectPath),
             "%s/benchmarks/cases/dispatch_loops/zr/benchmark_dispatch_loops.zrp",
             ZR_VM_TESTS_SOURCE_DIR);
    snprintf(sourcePath,
             sizeof(sourcePath),
             "%s/benchmarks/cases/dispatch_loops/zr/src/main.zr",
             ZR_VM_TESTS_SOURCE_DIR);

    global = ZrLibrary_CommonState_CommonGlobalState_New(projectPath);
    TEST_ASSERT_NOT_NULL(global);

    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(global));
    TEST_ASSERT_NOT_NULL(state->global->compileSource);

    source = read_text_file_owned(sourcePath);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_CreateFromNative(state, sourcePath);
    TEST_ASSERT_NOT_NULL(sourceName);

    function = state->global->compileSource(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    prepare_project_entry_runtime_module(state, function, sourceName);

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(320214929, result.value.nativeObject.nativeInt64);

    memset(&stepStats, 0, sizeof(stepStats));
    collect_member_callsite_stats_recursive(function,
                                            ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET,
                                            "step",
                                            0,
                                            &stepStats);
    format_member_callsite_stats("step", &stepStats, stepStatsMessage, sizeof(stepStatsMessage));

    TEST_ASSERT_TRUE_MESSAGE(stepStats.cacheEntryCount > 0u, stepStatsMessage);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(4u, stepStats.uniqueReceiverPrototypeCount, stepStatsMessage);
    TEST_ASSERT_TRUE_MESSAGE(stepStats.totalMissCount <= 8u, stepStatsMessage);
    TEST_ASSERT_TRUE_MESSAGE(stepStats.totalHitCount > stepStats.totalMissCount, stepStatsMessage);

    free(source);
    ZrCore_Function_Free(state, function);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    ZR_TEST_DIVIDER();
}

void test_gc_fragment_stress_benchmark_project_run_returns_expected_checksum(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "GC Fragment Stress Benchmark Project Run Returns Expected Checksum";
    char projectPath[512];
    SZrGlobalState *global = ZR_NULL;
    SZrState *state = ZR_NULL;
    SZrString *resultString;
    ZrProjectRunRequest request;
    EZrThreadStatus outerStatus;
    SZrTypeValue result;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("GC fragment stress benchmark project",
                 "Testing that the GC-heavy benchmark project importing local bench_config plus zr.system/zr.container runs without quickening a GC string slot into a *_PLAIN_DEST arithmetic destination.");

    snprintf(projectPath,
             sizeof(projectPath),
             "%s/benchmarks/cases/gc_fragment_stress/zr/benchmark_gc_fragment_stress.zrp",
             ZR_VM_TESTS_SOURCE_DIR);

    global = ZrLibrary_CommonState_CommonGlobalState_New(projectPath);
    TEST_ASSERT_NOT_NULL(global);

    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);

    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(global));
    TEST_ASSERT_TRUE(ZrVmLibContainer_Register(global));

    request.result = &result;
    ZrCore_Value_ResetAsNull(request.result);
    request.status = ZR_THREAD_STATUS_INVALID;
    outerStatus = ZrCore_Exception_TryRun(state, run_project_body, &request);

    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, outerStatus);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, request.status);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, request.result->type);
    resultString = ZR_CAST_STRING(state, request.result->value.object);
    TEST_ASSERT_NOT_NULL(resultString);
    TEST_ASSERT_EQUAL_STRING("BENCH_GC_FRAGMENT_STRESS_PASS\n857265678",
                             ZrCore_String_GetNativeString(resultString));

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    ZR_TEST_DIVIDER();
}

void test_matrix_add_2d_compile_avoids_adjacent_temp_reloads_before_super_array_int_ops(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Matrix Add 2D Compile Avoids Adjacent Temp Reloads Before Super Array Int Ops";
    ZrMatrixAdd2dCompileFixture fixture;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Compiler temp-forwarding peephole",
                 "Testing that matrix_add_2d no longer emits adjacent GET_STACK temp reloads directly feeding SUPER_ARRAY_GET_INT and SUPER_ARRAY_SET_INT");

    TEST_ASSERT_TRUE_MESSAGE(ZrTests_PrepareMatrixAdd2dCompileFixture(&fixture, "adjacent_temp_reload_before_super_array"),
                             "Failed to prepare fresh matrix_add_2d compile fixture");
    assert_super_array_int_ops_do_not_reload_adjacent_temp_slots(fixture.function, 0);
    TEST_ASSERT_TRUE_MESSAGE(
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST), 0) > 0,
            "matrix_add_2d should quicken the zero-fill loop into SUPER_ARRAY_FILL_INT4_CONST");

    ZrTests_FreeMatrixAdd2dCompileFixture(&fixture);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_matrix_add_2d_compile_folds_right_hand_int_constants_into_const_opcodes(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Matrix Add 2D Compile Folds Right Hand Int Constants Into Const Opcodes";
    ZrMatrixAdd2dCompileFixture fixture;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Const arithmetic quickening",
                 "Testing that matrix_add_2d lowers hot GET_CONSTANT plus int arithmetic chains into dedicated *_CONST opcodes instead of keeping the generic two-instruction form.");

    TEST_ASSERT_TRUE_MESSAGE(ZrTests_PrepareMatrixAdd2dCompileFixture(&fixture, "folds_right_hand_int_constants"),
                             "Failed to prepare fresh matrix_add_2d compile fixture");

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST), 0),
            "matrix_add_2d should quicken right-hand constant multiplies into MUL_SIGNED_CONST");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(ADD_INT_CONST), 0) +
                    count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST), 0) +
                    count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST), 0),
            "matrix_add_2d should quicken right-hand constant additions into the signed ADD_*_CONST family");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST), 0),
            "matrix_add_2d should quicken right-hand constant mod operations into MOD_SIGNED_CONST");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST), 0),
            "matrix_add_2d should quicken right-hand constant divisions into DIV_SIGNED_CONST");

    ZrTests_FreeMatrixAdd2dCompileFixture(&fixture);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_matrix_add_2d_compile_eliminates_temp_self_updates_for_add_int_const(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Matrix Add 2D Compile Eliminates Temp Self Updates For Add Int Const";
    ZrMatrixAdd2dCompileFixture fixture;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Loop self-update peephole",
                 "Testing that matrix_add_2d no longer keeps GET_STACK plus ADD_INT_CONST plus SET_STACK temp chains when the updated value is written back to the same slot.");

    TEST_ASSERT_TRUE_MESSAGE(ZrTests_PrepareMatrixAdd2dCompileFixture(&fixture, "eliminates_temp_self_updates"),
                             "Failed to prepare fresh matrix_add_2d compile fixture");

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            0u,
            count_stack_self_update_int_const_triplets_recursive(fixture.function, 0),
            "matrix_add_2d should fold ADD_INT_CONST self-updates into direct destination writes");

    ZrTests_FreeMatrixAdd2dCompileFixture(&fixture);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_matrix_add_2d_compile_folds_loop_bounds_into_sub_int_const(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Matrix Add 2D Compile Folds Loop Bounds Into Sub Int Const";
    ZrMatrixAdd2dCompileFixture fixture;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Loop bound const fold",
                 "Testing that matrix_add_2d no longer leaves GET_CONSTANT int plus SUB_INT pairs in loop headers once the constant is consumed only as a right-hand operand.");

    TEST_ASSERT_TRUE_MESSAGE(ZrTests_PrepareMatrixAdd2dCompileFixture(&fixture, "folds_loop_bounds_into_sub_int_const"),
                             "Failed to prepare fresh matrix_add_2d compile fixture");

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(SUB_INT_CONST), 0) +
                    count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST), 0) +
                    count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST), 0),
            "matrix_add_2d should emit the signed SUB_*_CONST family for loop-bound arithmetic");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            0u,
            count_sub_int_right_constant_pairs_recursive(fixture.function, 0),
            "matrix_add_2d should fold right-hand constant loop-bound SUB_INT pairs into SUB_INT_CONST");

    ZrTests_FreeMatrixAdd2dCompileFixture(&fixture);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_matrix_add_2d_compile_eliminates_forwardable_get_stack_copy_reads(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Matrix Add 2D Compile Eliminates Forwardable Get Stack Copy Reads";
    ZrMatrixAdd2dCompileFixture fixture;
    const SZrFunction *ownerFunction = ZR_NULL;
    TZrUInt32 fillInstructionIndex = UINT32_MAX;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("GET_STACK copy propagation",
                 "Testing that matrix_add_2d no longer keeps forwardable GET_STACK temp copies in its hot loops when later supported readers can consume the original source slot directly.");

    TEST_ASSERT_TRUE_MESSAGE(ZrTests_PrepareMatrixAdd2dCompileFixture(&fixture, "eliminates_forwardable_get_stack_copy_reads"),
                             "Failed to prepare fresh matrix_add_2d compile fixture");

    TEST_ASSERT_TRUE(find_first_opcode_recursive(fixture.function,
                                                 ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST),
                                                 0,
                                                 &ownerFunction,
                                                 &fillInstructionIndex));
    TEST_ASSERT_NOT_NULL(ownerFunction);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            0u,
            count_forwardable_get_stack_copy_reads_in_function_range(ownerFunction, fillInstructionIndex + 1u),
            "matrix_add_2d hot paths should not keep same-block GET_STACK temp copies when later supported readers can directly consume the source slot");

    ZrTests_FreeMatrixAdd2dCompileFixture(&fixture);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_matrix_add_2d_compile_fuses_less_equal_signed_jump_if_loop_guards(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Matrix Add 2D Compile Fuses Less Equal Signed Jump If Loop Guards";
    ZrMatrixAdd2dCompileFixture fixture;
    const SZrFunction *ownerFunction = ZR_NULL;
    TZrUInt32 fillInstructionIndex = UINT32_MAX;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Compare plus branch fusion",
                 "Testing that matrix_add_2d no longer emits adjacent LOGICAL_LESS_EQUAL_SIGNED plus JUMP_IF loop guards once the compare result only feeds the branch.");

    TEST_ASSERT_TRUE_MESSAGE(ZrTests_PrepareMatrixAdd2dCompileFixture(&fixture, "fuses_less_equal_signed_jump_if"),
                             "Failed to prepare fresh matrix_add_2d compile fixture");

    TEST_ASSERT_TRUE(find_first_opcode_recursive(fixture.function,
                                                 ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST),
                                                 0,
                                                 &ownerFunction,
                                                 &fillInstructionIndex));
    TEST_ASSERT_NOT_NULL(ownerFunction);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            0u,
            count_less_equal_signed_jump_if_pairs_in_function_range(ownerFunction, fillInstructionIndex + 1u),
            "matrix_add_2d hot loop should not keep adjacent LOGICAL_LESS_EQUAL_SIGNED plus JUMP_IF guard pairs");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_jump_if_greater_signed_in_function_range(ownerFunction, fillInstructionIndex + 1u),
            "matrix_add_2d hot loop should emit the fused JUMP_IF_GREATER_SIGNED guard opcode");

    ZrTests_FreeMatrixAdd2dCompileFixture(&fixture);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_matrix_add_2d_compile_folds_direct_result_stores_into_final_slots(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Matrix Add 2D Compile Folds Direct Result Stores Into Final Slots";
    ZrMatrixAdd2dCompileFixture fixture;
    const SZrFunction *ownerFunction = ZR_NULL;
    TZrUInt32 fillInstructionIndex = UINT32_MAX;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Pure result plus SET_STACK fold",
                 "Testing that matrix_add_2d no longer keeps pure int-result producers followed immediately by SET_STACK copies inside the hot loop after SUPER_ARRAY_FILL_INT4_CONST.");

    TEST_ASSERT_TRUE_MESSAGE(ZrTests_PrepareMatrixAdd2dCompileFixture(&fixture, "folds_direct_result_stores"),
                             "Failed to prepare fresh matrix_add_2d compile fixture");

    TEST_ASSERT_TRUE(find_first_opcode_recursive(fixture.function,
                                                 ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST),
                                                 0,
                                                 &ownerFunction,
                                                 &fillInstructionIndex));
    TEST_ASSERT_NOT_NULL(ownerFunction);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            0u,
            count_direct_result_store_followed_by_set_stack_pairs_in_function_range(ownerFunction,
                                                                                    fillInstructionIndex + 1u),
            "matrix_add_2d hot loop should fold pure int-result temporaries directly into their final destination slots");

    ZrTests_FreeMatrixAdd2dCompileFixture(&fixture);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_matrix_add_2d_compile_eliminates_zero_init_constant_copy_pairs(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Matrix Add 2D Compile Eliminates Zero Init Constant Copy Pairs";
    ZrMatrixAdd2dCompileFixture fixture;
    const SZrFunction *ownerFunction = ZR_NULL;
    TZrUInt32 fillInstructionIndex = UINT32_MAX;
    TZrInt32 zeroConstantIndex;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Loop zero-init constant copy cleanup",
                 "Testing that matrix_add_2d no longer keeps GET_CONSTANT(0) plus SET_STACK pairs when loop counters can be initialized directly in their final local slots.");

    TEST_ASSERT_TRUE_MESSAGE(ZrTests_PrepareMatrixAdd2dCompileFixture(&fixture, "eliminates_zero_init_constant_copy_pairs"),
                             "Failed to prepare fresh matrix_add_2d compile fixture");

    zeroConstantIndex = find_top_level_int_constant_index(fixture.function, 0);
    TEST_ASSERT_TRUE_MESSAGE(zeroConstantIndex >= 0, "Expected matrix_add_2d constant pool to contain int 0");
    TEST_ASSERT_TRUE(find_first_opcode_recursive(fixture.function,
                                                 ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST),
                                                 0,
                                                 &ownerFunction,
                                                 &fillInstructionIndex));
    TEST_ASSERT_NOT_NULL(ownerFunction);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            0u,
            count_get_constant_set_stack_pairs_for_constant_in_function_range(ownerFunction,
                                                                              fillInstructionIndex + 1u,
                                                                              (TZrUInt32)zeroConstantIndex),
            "matrix_add_2d hot loop should initialize zero-valued loop locals directly instead of copying through temp slots");

    ZrTests_FreeMatrixAdd2dCompileFixture(&fixture);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_matrix_add_2d_compile_eliminates_generic_array_int_index_opcodes(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Matrix Add 2D Compile Eliminates Generic Array Int Index Opcodes";
    char diagnostic[2048];
    ZrMatrixAdd2dCompileFixture fixture;
    TZrUInt32 genericGetCount;
    TZrUInt32 genericSetCount;
    TZrUInt32 fastGetCount;
    TZrUInt32 plainFastGetCount;
    TZrUInt32 fastSetCount;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Array<int> index quickening coverage",
                 "Testing that matrix_add_2d no longer leaves generic GET_BY_INDEX/SET_BY_INDEX opcodes for its typed Array<int> hot paths.");

    TEST_ASSERT_TRUE_MESSAGE(ZrTests_PrepareMatrixAdd2dCompileFixture(&fixture, "eliminates_generic_array_int_index_opcodes"),
                             "Failed to prepare fresh matrix_add_2d compile fixture");

    genericGetCount = count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(GET_BY_INDEX), 0);
    genericSetCount = count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(SET_BY_INDEX), 0);
    plainFastGetCount =
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST), 0) +
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST), 0);
    fastGetCount = count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT), 0) +
                   count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS), 0) +
                   plainFastGetCount;
    fastSetCount = count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT), 0) +
                   count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT_ITEMS), 0);

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            fastGetCount,
            "matrix_add_2d should emit SUPER_ARRAY_GET_INT-family opcodes for typed Array<int> reads");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            fastSetCount,
            "matrix_add_2d should emit SUPER_ARRAY_SET_INT-family opcodes for typed Array<int> writes");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            plainFastGetCount,
            "matrix_add_2d should emit SUPER_ARRAY_GET_INT_PLAIN_DEST-family opcodes for plain int temporaries on its hot read path");
    if (fastGetCount > 5u) {
        TEST_FAIL_MESSAGE("matrix_add_2d should forward immediate super-array read-after-write pairs and keep its hot fast get count at 5 or below");
    }
    if (genericGetCount != 0) {
        build_opcode_window_message(fixture.function, ZR_INSTRUCTION_ENUM(GET_BY_INDEX), diagnostic, sizeof(diagnostic));
        TEST_FAIL_MESSAGE(diagnostic);
    }
    if (genericSetCount != 0) {
        build_opcode_window_message(fixture.function, ZR_INSTRUCTION_ENUM(SET_BY_INDEX), diagnostic, sizeof(diagnostic));
        TEST_FAIL_MESSAGE(diagnostic);
    }

    ZrTests_FreeMatrixAdd2dCompileFixture(&fixture);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_matrix_add_2d_compile_emits_plain_destination_int_arithmetic_opcodes(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Matrix Add 2D Compile Emits Plain Destination Int Arithmetic Opcodes";
    ZrMatrixAdd2dCompileFixture fixture;
    TZrUInt32 plainArithmeticCount;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Plain-destination arithmetic promotion",
                 "Testing that matrix_add_2d emits arithmetic *_PLAIN_DEST opcodes once plain temporary destinations are proven reusable.");

    TEST_ASSERT_TRUE_MESSAGE(
            ZrTests_PrepareMatrixAdd2dCompileFixture(&fixture, "emits_plain_destination_int_arithmetic_opcodes"),
            "Failed to prepare fresh matrix_add_2d compile fixture");

    plainArithmeticCount =
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST), 0) +
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST), 0) +
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST), 0) +
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST), 0) +
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST), 0) +
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST), 0) +
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST), 0) +
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST), 0) +
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST), 0) +
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST), 0) +
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST), 0) +
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST), 0) +
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST), 0) +
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST), 0) +
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST), 0) +
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST), 0) +
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST), 0) +
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST_PLAIN_DEST), 0) +
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST), 0) +
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST_PLAIN_DEST), 0);

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            plainArithmeticCount,
            "matrix_add_2d should emit arithmetic *_PLAIN_DEST opcodes after plain-destination quickening");

    ZrTests_FreeMatrixAdd2dCompileFixture(&fixture);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static TZrUInt32 count_typed_signed_add_family_recursive(const SZrFunction *function, TZrUInt32 depth) {
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE_MESSAGE(depth < 64, "Typed signed add recursion depth exceeded 64");

    return count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST), depth);
}

static TZrUInt32 count_typed_unsigned_add_family_recursive(const SZrFunction *function, TZrUInt32 depth) {
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE_MESSAGE(depth < 64, "Typed unsigned add recursion depth exceeded 64");

    return count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_UNSIGNED), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST), depth);
}

static TZrUInt32 count_typed_signed_sub_family_recursive(const SZrFunction *function, TZrUInt32 depth) {
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE_MESSAGE(depth < 64, "Typed signed sub recursion depth exceeded 64");

    return count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUB_SIGNED), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST), depth);
}

static TZrUInt32 count_typed_unsigned_sub_family_recursive(const SZrFunction *function, TZrUInt32 depth) {
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE_MESSAGE(depth < 64, "Typed unsigned sub recursion depth exceeded 64");

    return count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUB_UNSIGNED), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST), depth);
}

static TZrUInt32 count_typed_signed_plain_destination_family_recursive(const SZrFunction *function, TZrUInt32 depth) {
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE_MESSAGE(depth < 64, "Typed signed plain-destination recursion depth exceeded 64");

    return count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST), depth) +
           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST), depth);
}

void test_strongly_typed_compile_prefers_typed_arithmetic_and_equality_opcodes(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Strongly Typed Compile Prefers Typed Arithmetic And Equality Opcodes";
    const char *source =
            "var si: int = 7;\n"
            "var sj: int = 2;\n"
            "var su: uint = 9;\n"
            "var sv: uint = 4;\n"
            "var sb: bool = true;\n"
            "var ss: string = \"left\";\n"
            "var st: string = \"left\";\n"
            "var sf: float = 1.5;\n"
            "var sg: float = 1.5;\n"
            "var signedSum: int = si + sj;\n"
            "var signedDiff: int = si - sj;\n"
            "var unsignedSum: uint = su + sv;\n"
            "var unsignedDiff: uint = su - sv;\n"
            "var boolEq: bool = sb == true;\n"
            "var signedNe: bool = si != sj;\n"
            "var unsignedNe: bool = su != sv;\n"
            "var floatEq: bool = sf == sg;\n"
            "var stringEq: bool = ss == st;\n"
            "return signedSum + signedDiff + <int> unsignedSum + <int> unsignedDiff +\n"
            "       (boolEq ? 1 : 0) + (signedNe ? 1 : 0) + (unsignedNe ? 1 : 0) +\n"
            "       (floatEq ? 1 : 0) + (stringEq ? 1 : 0);\n";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Typed opcode emission",
                 "Testing that strongly typed signed/unsigned arithmetic and bool/string/float equality lower to dedicated typed opcodes instead of ADD_INT/SUB_INT/LOGICAL_EQUAL fallback opcodes.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);

    sourceName = ZrCore_String_CreateFromNative(state, "typed_opcode_emission_regression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_typed_signed_add_family_recursive(function, 0),
            "Strongly typed signed additions should lower to ADD_SIGNED-family opcodes");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_typed_signed_sub_family_recursive(function, 0),
            "Strongly typed signed subtractions should lower to SUB_SIGNED-family opcodes");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_typed_unsigned_add_family_recursive(function, 0),
            "Strongly typed unsigned additions should lower to ADD_UNSIGNED-family opcodes");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_typed_unsigned_sub_family_recursive(function, 0),
            "Strongly typed unsigned subtractions should lower to SUB_UNSIGNED-family opcodes");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL), 0),
            "Strongly typed bool equality should lower to LOGICAL_EQUAL_BOOL");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_SIGNED), 0),
            "Strongly typed signed inequality should lower to LOGICAL_NOT_EQUAL_SIGNED");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_UNSIGNED), 0),
            "Strongly typed unsigned inequality should lower to LOGICAL_NOT_EQUAL_UNSIGNED");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_FLOAT), 0),
            "Strongly typed float equality should lower to LOGICAL_EQUAL_FLOAT");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING), 0),
            "Strongly typed string equality should lower to LOGICAL_EQUAL_STRING");

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_INT), 0) +
                    count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST), 0) +
                    count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_INT_CONST), 0) +
                    count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST), 0),
            "Strongly typed signed/unsigned arithmetic should not fall back to ADD_INT-family opcodes");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUB_INT), 0) +
                    count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST), 0) +
                    count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUB_INT_CONST), 0) +
                    count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST), 0),
            "Strongly typed signed/unsigned arithmetic should not fall back to SUB_INT-family opcodes");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL), 0) +
                    count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL), 0),
            "Strongly typed equality comparisons should not fall back to LOGICAL_EQUAL/LOGICAL_NOT_EQUAL");

    ZrCore_Function_Free(state, function);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_typed_quickening_promotes_const_and_plain_destination_variants(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Typed Quickening Promotes Const And Plain Destination Variants";
    const char *source =
            "var si: int = 7;\n"
            "var su: uint = 9;\n"
            "var signedChain: int = (si + 1) + 2;\n"
            "var unsignedChain: uint = (su - <uint>1) - <uint>2;\n"
            "return signedChain + <int> unsignedChain;\n";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Typed const/plain-dest quickening",
                 "Testing that typed signed/unsigned arithmetic keeps narrowing after frontend emission and reaches dedicated *_CONST and *_PLAIN_DEST variants instead of stopping at ADD_SIGNED/SUB_UNSIGNED.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);

    sourceName = ZrCore_String_CreateFromNative(state, "typed_quickening_const_plain_dest_regression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST), 0) +
                    count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST), 0),
            "Typed signed arithmetic with right-hand constants should quicken into ADD_SIGNED_CONST-family opcodes");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_typed_signed_plain_destination_family_recursive(function, 0),
            "Typed signed arithmetic should also reach a signed *_PLAIN_DEST variant once a reusable plain temporary exists");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST), 0) +
                    count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST), 0),
            "Typed unsigned arithmetic with right-hand constants should quicken into SUB_UNSIGNED_CONST-family opcodes");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST), 0),
            "Typed unsigned temp arithmetic should promote to SUB_UNSIGNED_CONST_PLAIN_DEST");

    ZrCore_Function_Free(state, function);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_known_native_calls_quicken_to_dedicated_call_family(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Known Native Calls Quicken To Dedicated Call Family";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;
    char *source = ZR_NULL;
    char sourcePath[1024];

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Known native call quickening",
                 "Testing that decorator helper call sites lower to KNOWN_NATIVE_CALL instead of falling back to the generic FUNCTION_CALL path.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);

    snprintf(sourcePath,
             sizeof(sourcePath),
             "%s/fixtures/scripts/decorator_artifact_baseline.zr",
             ZR_VM_TESTS_SOURCE_DIR);
    source = read_text_file_owned(sourcePath);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_CreateFromNative(state, "decorator_artifact_baseline.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL), 0),
            "Decorator helper call sites should quicken to KNOWN_NATIVE_CALL");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL), 0) +
                    count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS), 0) +
                    count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS), 0),
            "Decorator fixture should stay on the fixed-arity KNOWN_NATIVE_CALL opcode in this regression");

    ZrCore_Function_Free(state, function);
    free(source);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_known_native_member_calls_quicken_to_dedicated_member_call_opcode(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Known Native Member Calls Quicken To Dedicated Member Call Opcode";
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var seen = new container.Set<int>();\n"
            "var score = 0;\n"
            "if (seen.add(7)) { score = score + 10; }\n"
            "if (!seen.add(7)) { score = score + 20; }\n"
            "if (seen.add(11)) { score = score + 30; }\n"
            "return seen.count * 100 + score;\n";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;
    TZrInt64 result = 0;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("known native member-call quickening",
                 "Testing that typed native member calls lower directly to KNOWN_NATIVE_MEMBER_CALL instead of keeping GET_MEMBER_SLOT plus KNOWN_NATIVE_CALL.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE_MESSAGE(ZrVmLibContainer_Register(state->global),
                             "Failed to register zr.container for native member-call test");

    sourceName = ZrCore_String_CreateFromNative(state, "known_native_member_call_quickening_regression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL), 0),
            "Set.add calls should quicken to KNOWN_NATIVE_MEMBER_CALL");
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(260, result);

    ZrCore_Function_Free(state, function);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_static_native_box_member_call_executes_without_receiver_frame_rewrite(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Static Native Box Member Call Executes Without Receiver Frame Rewrite";
    const char *source =
            "var builtin = %import(\"zr.builtin\");\n"
            "var {TypeInfo} = %import(\"zr.builtin\");\n"
            "var left = builtin.Object.box(7);\n"
            "var right = TypeInfo.box(7);\n"
            "return left.hashCode() == right.hashCode();\n";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;
    SZrTypeValue result;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("static native member-call receiver frame",
                 "Testing that static native helpers such as TypeInfo.box keep the normal native call frame instead of being rewritten into an instance member-call frame.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_NativeRegistry_Attach(state->global),
                             "Failed to attach builtin native module registry");

    sourceName = ZrCore_String_CreateFromNative(state, "static_native_box_member_call_regression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(
            ZrTests_Runtime_Function_Execute(state, function, &result),
            "TypeInfo.box should execute as a static native call and preserve boxed helper runtime semantics");
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, result.type);
    TEST_ASSERT_TRUE(result.value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE);

    ZrCore_Function_Free(state, function);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_known_vm_member_call_load1_quickening_fuses_receiver_and_argument_loads(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Known VM Member Call Load1 Quickening Fuses Receiver And Argument Loads";
    char projectPath[1024];
    char sourcePath[1024];
    char *source = ZR_NULL;
    SZrGlobalState *global;
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;
    TZrUInt32 fusedCallCount;
    char fusedHits[1024];
    char failureMessage[1536];

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("known VM member-call load1 fusion",
                 "Testing that receiver and one argument GET_STACK materialization before a direct VM member call fuse into one opcode.");

    snprintf(projectPath,
             sizeof(projectPath),
             "%s/benchmarks/cases/dispatch_loops/zr/benchmark_dispatch_loops.zrp",
             ZR_VM_TESTS_SOURCE_DIR);
    snprintf(sourcePath,
             sizeof(sourcePath),
             "%s/benchmarks/cases/dispatch_loops/zr/src/main.zr",
             ZR_VM_TESTS_SOURCE_DIR);

    global = ZrLibrary_CommonState_CommonGlobalState_New(projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(global));

    source = read_text_file_owned(sourcePath);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_CreateFromNative(state, sourcePath);
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    fusedCallCount = count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL_LOAD1_U8), 0);
    build_opcode_hits_message(function,
                              ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL_LOAD1_U8),
                              fusedHits,
                              sizeof(fusedHits));
    snprintf(failureMessage,
             sizeof(failureMessage),
             "KNOWN_VM_MEMBER_CALL_LOAD1_U8=%u hits=%s",
             (unsigned int)fusedCallCount,
             fusedHits);

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0u, fusedCallCount, failureMessage);

    ZrCore_Function_Free(state, function);
    free(source);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    ZR_TEST_DIVIDER();
}

void test_typed_member_calls_quicken_to_known_vm_call_family(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Typed Member Calls Quicken To Known VM Call Family";
    const char *source =
            "class Counter {\n"
            "    pub var value: int;\n"
            "    pub step(delta: int): int {\n"
            "        this.value = this.value + delta;\n"
            "        return this.value;\n"
            "    }\n"
            "    pub read(): int {\n"
            "        return this.value;\n"
            "    }\n"
            "}\n"
            "var counter = new Counter();\n"
            "counter.value = 3;\n"
            "var first = counter.step(4);\n"
            "var second = counter.read();\n"
            "return first + second;\n";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;
    const SZrFunctionMemberEntry *stepEntry;
    const SZrFunctionMemberEntry *readEntry;
    TZrInt64 result = 0;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("typed member direct known-vm member-call lowering",
                 "Testing that typed instance method calls lower into the direct KNOWN_VM_MEMBER_CALL opcode instead of paying a separate GET_MEMBER_SLOT plus KNOWN_VM_CALL pair.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);

    sourceName = ZrCore_String_CreateFromNative(state, "typed_member_known_call_quickening_regression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            1u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL), 0),
            "Typed method calls, including receiver-bound zero-arg methods, should quicken to KNOWN_VM_MEMBER_CALL");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL), 0) +
                    count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS), 0),
            "Typed method calls should no longer retain the older GET_MEMBER_SLOT plus KNOWN_VM_CALL lowering");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(FUNCTION_CALL), 0) +
                    count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS), 0),
            "Typed method calls should not fall back to the generic FUNCTION_CALL family");
    stepEntry = find_member_entry_by_symbol(function, "step");
    readEntry = find_member_entry_by_symbol(function, "read");
    TEST_ASSERT_NOT_NULL_MESSAGE(stepEntry, "Typed member quickening fixture must retain a member entry for step");
    TEST_ASSERT_NOT_NULL_MESSAGE(readEntry, "Typed member quickening fixture must retain a member entry for read");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(
            ZR_FUNCTION_MEMBER_ENTRY_KIND_BOUND_DESCRIPTOR,
            stepEntry->entryKind,
            "Typed member slot entries should bind the runtime descriptor at compile time for step");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(
            ZR_FUNCTION_MEMBER_ENTRY_KIND_BOUND_DESCRIPTOR,
            readEntry->entryKind,
            "Typed member slot entries should bind the runtime descriptor at compile time for read");
    TEST_ASSERT_TRUE_MESSAGE(stepEntry->prototypeIndex < function->prototypeCount,
                             "step member entry should point at a serialized runtime prototype");
    TEST_ASSERT_TRUE_MESSAGE(readEntry->prototypeIndex < function->prototypeCount,
                             "read member entry should point at a serialized runtime prototype");

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(14, result);

    ZrCore_Function_Free(state, function);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_typed_member_call_initializers_bind_directly_into_local_slots(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Typed Member Call Initializers Bind Directly Into Local Slots";
    const char *source =
            "class Counter {\n"
            "    pub var value: int;\n"
            "    pub step(delta: int): int {\n"
            "        this.value = this.value + delta;\n"
            "        return this.value;\n"
            "    }\n"
            "    pub read(): int {\n"
            "        return this.value;\n"
            "    }\n"
            "}\n"
            "var counter = new Counter();\n"
            "counter.value = 3;\n"
            "var first = counter.step(4);\n"
            "var second = counter.read();\n"
            "return first + second;\n";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;
    TZrInt64 result = 0;
    TZrUInt32 knownVmMemberCallCount;
    TZrUInt32 directResultSetStackPairs;
    char knownVmMemberCallHits[1024];
    char failureMessage[2048];

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("typed member-call initializer local binding",
                 "Testing that typed member-call initializers compile directly into their reserved local slots instead of emitting KNOWN_VM_MEMBER_CALL immediately followed by SET_STACK.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);

    sourceName = ZrCore_String_CreateFromNative(state, "typed_member_call_initializer_slot_binding_regression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    knownVmMemberCallCount = count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL), 0);
    directResultSetStackPairs = count_known_vm_member_call_result_store_pairs_in_function_range(function, 0);
    build_opcode_hits_message(function,
                              ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL),
                              knownVmMemberCallHits,
                              sizeof(knownVmMemberCallHits));
    snprintf(failureMessage,
             sizeof(failureMessage),
             "known-vm-member-call=%u direct-result-set-stack-pairs=%u hits=%s",
             (unsigned int)knownVmMemberCallCount,
             (unsigned int)directResultSetStackPairs,
             knownVmMemberCallHits);

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            1u,
            knownVmMemberCallCount,
            "Fixture must keep the typed member-call fast path active before checking direct local-slot binding");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, directResultSetStackPairs, failureMessage);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(14, result);

    ZrCore_Function_Free(state, function);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_typed_member_call_binary_operands_bind_directly_into_operand_slots(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Typed Member Call Binary Operands Bind Directly Into Operand Slots";
    const char *source =
            "class Counter {\n"
            "    pub var value: int;\n"
            "    pub read(): int {\n"
            "        return this.value;\n"
            "    }\n"
            "}\n"
            "var counter = new Counter();\n"
            "counter.value = 3;\n"
            "return counter.read() * 2;\n";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;
    TZrInt64 result = 0;
    TZrUInt32 knownVmMemberCallCount;
    TZrUInt32 receiverCopyTriplets;
    char knownVmMemberCallHits[1024];
    char failureMessage[2048];

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("typed member-call binary operand binding",
                 "Testing that typed zero-arg member calls used as binary operands compile directly into their operand slots instead of keeping GET_STACK plus SET_STACK receiver copies before KNOWN_VM_MEMBER_CALL.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);

    sourceName = ZrCore_String_CreateFromNative(state, "typed_member_call_binary_operand_slot_binding_regression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    knownVmMemberCallCount = count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL), 0);
    receiverCopyTriplets = count_known_vm_member_call_receiver_copy_triplets_in_function_range(function, 0);
    build_opcode_hits_message(function,
                              ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL),
                              knownVmMemberCallHits,
                              sizeof(knownVmMemberCallHits));
    snprintf(failureMessage,
             sizeof(failureMessage),
             "known-vm-member-call=%u receiver-copy-triplets=%u hits=%s",
             (unsigned int)knownVmMemberCallCount,
             (unsigned int)receiverCopyTriplets,
             knownVmMemberCallHits);

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            knownVmMemberCallCount,
            "Fixture must keep the typed member-call fast path active before checking binary operand slot binding");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, receiverCopyTriplets, failureMessage);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(6, result);

    ZrCore_Function_Free(state, function);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_nested_argument_calls_do_not_reuse_tail_call_lowering(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Nested Argument Calls Do Not Reuse Tail Call Lowering";
    const char *source =
            "func callLeaf(value: int, salt: int): int {\n"
            "    return (value * 17 + salt * 13 + 19) % 100003;\n"
            "}\n"
            "func callChainA(value: int, salt: int): int {\n"
            "    return callLeaf(value + 3, salt + 1);\n"
            "}\n"
            "func callChainB(value: int, salt: int): int {\n"
            "    return callLeaf(callChainA(value + salt % 5, salt + 7), salt + 11);\n"
            "}\n"
            "return callChainB(445, 10);\n";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;
    const SZrFunction *callChainBFunction;
    TZrUInt32 tailCallCount;
    TZrUInt32 nonTailCallCount;
    char tailCallHits[1024];
    char nonTailCallHits[1024];
    char failureMessage[2048];
    TZrInt64 result = 0;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("nested-call tail lowering fence",
                 "Testing that an inner call used as an argument stays a normal call, while only the outer return-position call remains tail-lowered.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);

    sourceName = ZrCore_String_CreateFromNative(state, "nested_argument_tail_call_regression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    callChainBFunction = find_child_function_by_name_recursive(function, "callChainB", 0);
    TEST_ASSERT_NOT_NULL_MESSAGE(callChainBFunction, "Fixture must retain a compiled child function named callChainB");

    tailCallCount = count_tail_call_family_recursive(callChainBFunction, 0);
    nonTailCallCount = count_non_tail_call_family_recursive(callChainBFunction, 0);
    build_opcode_hits_message(callChainBFunction,
                              ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL),
                              tailCallHits,
                              sizeof(tailCallHits));
    build_opcode_hits_message(callChainBFunction,
                              ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL),
                              nonTailCallHits,
                              sizeof(nonTailCallHits));
    snprintf(failureMessage,
             sizeof(failureMessage),
             "callChainB tailCalls=%u nonTailCalls=%u tailHits=%s nonTailHits=%s",
             (unsigned int)tailCallCount,
             (unsigned int)nonTailCallCount,
             tailCallHits,
             nonTailCallHits);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, tailCallCount, failureMessage);
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0u, nonTailCallCount, failureMessage);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(34062, result);

    ZrCore_Function_Free(state, function);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_known_vm_call_results_keep_typed_arithmetic_specialization(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Known VM Call Results Keep Typed Arithmetic Specialization";
    const char *source =
            "class Producer {\n"
            "    pub step(delta: int): int {\n"
            "        return delta + 1;\n"
            "    }\n"
            "    pub read(): int {\n"
            "        return 5;\n"
            "    }\n"
            "}\n"
            "var producer = new Producer();\n"
            "return producer.step(4) + producer.read();\n";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;
    TZrInt64 result = 0;
    TZrUInt32 knownVmLikeCallCount;
    TZrUInt32 genericAddCount;
    TZrUInt32 signedAddCount;
    TZrBool prototypesMaterialized = ZR_FALSE;
    SZrObjectPrototype *producerPrototype = ZR_NULL;
    SZrString *stepName = ZR_NULL;
    SZrString *readName = ZR_NULL;
    SZrTypeValue memberKey;
    const SZrTypeValue *stepCallable = ZR_NULL;
    const SZrTypeValue *readCallable = ZR_NULL;
    SZrFunction *stepCallableMetadata = ZR_NULL;
    SZrFunction *readCallableMetadata = ZR_NULL;
    char genericAddHits[1024];
    char signedAddHits[1024];
    char knownVmCallHits[1024];
    char knownVmMemberCallHits[1024];
    char failureMessage[4096];

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("known-vm member-call result slot typing",
                 "Testing that arithmetic consuming typed member-call results keeps the signed ADD family instead of falling back to generic ADD.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);

    sourceName = ZrCore_String_CreateFromNative(state, "known_vm_call_result_arithmetic_specialization_regression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    knownVmLikeCallCount = count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL), 0) +
                           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS), 0) +
                           count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL), 0);
    genericAddCount = count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD), 0);
    signedAddCount = count_typed_signed_add_family_recursive(function, 0);

    prototypesMaterialized = ZrCore_Module_CreatePrototypesFromData(state, ZR_NULL, function);
    if (prototypesMaterialized &&
        function->prototypeInstances != ZR_NULL &&
        function->prototypeInstancesLength > 0) {
        producerPrototype = function->prototypeInstances[0];
    }
    if (producerPrototype != ZR_NULL) {
        stepName = ZrCore_String_CreateFromNative(state, "step");
        readName = ZrCore_String_CreateFromNative(state, "read");
        if (stepName != ZR_NULL) {
            ZrCore_Value_InitAsRawObject(state, &memberKey, ZR_CAST_RAW_OBJECT_AS_SUPER(stepName));
            memberKey.type = ZR_VALUE_TYPE_STRING;
            stepCallable = ZrCore_Object_GetValue(state, &producerPrototype->super, &memberKey);
            stepCallableMetadata = ZrCore_Closure_GetMetadataFunctionFromValue(state, stepCallable);
        }
        if (readName != ZR_NULL) {
            ZrCore_Value_InitAsRawObject(state, &memberKey, ZR_CAST_RAW_OBJECT_AS_SUPER(readName));
            memberKey.type = ZR_VALUE_TYPE_STRING;
            readCallable = ZrCore_Object_GetValue(state, &producerPrototype->super, &memberKey);
            readCallableMetadata = ZrCore_Closure_GetMetadataFunctionFromValue(state, readCallable);
        }
    }

    build_opcode_hits_message(function, ZR_INSTRUCTION_ENUM(ADD), genericAddHits, sizeof(genericAddHits));
    build_opcode_hits_message(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED), signedAddHits, sizeof(signedAddHits));
    build_opcode_hits_message(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL), knownVmCallHits, sizeof(knownVmCallHits));
    build_opcode_hits_message(function,
                              ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL),
                              knownVmMemberCallHits,
                              sizeof(knownVmMemberCallHits));

    snprintf(failureMessage,
             sizeof(failureMessage),
             "known-vm-like-call-count=%u generic-add=%u signed-add-family=%u | generic-add-hits: %s | signed-add-hits: %s | known-vm-call-hits: %s | known-vm-member-call-hits: %s",
             (unsigned int)knownVmLikeCallCount,
             (unsigned int)genericAddCount,
             (unsigned int)signedAddCount,
             genericAddHits,
             signedAddHits,
             knownVmCallHits,
             knownVmMemberCallHits);
    snprintf(failureMessage + strlen(failureMessage),
             sizeof(failureMessage) - strlen(failureMessage),
             " | prototype-materialized=%d prototype=%p | step-callable(type=%d native=%d meta=%p hasReturn=%d) | read-callable(type=%d native=%d meta=%p hasReturn=%d)",
             prototypesMaterialized ? 1 : 0,
             (void *)producerPrototype,
             stepCallable != ZR_NULL ? (int)stepCallable->type : -1,
             stepCallable != ZR_NULL ? (int)stepCallable->isNative : -1,
             (void *)stepCallableMetadata,
             stepCallableMetadata != ZR_NULL ? (int)stepCallableMetadata->hasCallableReturnType : -1,
             readCallable != ZR_NULL ? (int)readCallable->type : -1,
             readCallable != ZR_NULL ? (int)readCallable->isNative : -1,
             (void *)readCallableMetadata,
             readCallableMetadata != ZR_NULL ? (int)readCallableMetadata->hasCallableReturnType : -1);

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            knownVmLikeCallCount,
            "Fixture must quicken typed member calls into the KNOWN_VM_CALL or KNOWN_VM_MEMBER_CALL family before testing arithmetic specialization");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(FUNCTION_CALL), 0) +
                    count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS), 0),
            "Typed member call arithmetic fixture should not retain generic FUNCTION_CALL opcodes");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, genericAddCount, failureMessage);
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0u, signedAddCount, failureMessage);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(10, result);

    ZrCore_Function_Free(state, function);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_direct_child_function_calls_quicken_to_known_vm_call_family(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Direct Child Function Calls Quicken To Known VM Call Family";
    const char *source =
            "labelFor(slot: int): string {\n"
            "    var normalized = slot % 4;\n"
            "    if (normalized == 0) {\n"
            "        return \"aa\";\n"
            "    }\n"
            "    if (normalized == 1) {\n"
            "        return \"bb\";\n"
            "    }\n"
            "    if (normalized == 2) {\n"
            "        return \"cc\";\n"
            "    }\n"
            "    return \"dd\";\n"
            "}\n"
            "var key = labelFor(2 + 4);\n"
            "if (key == \"cc\") {\n"
            "    return 1;\n"
            "}\n"
            "return 0;\n";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;
    TZrInt64 result = 0;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("direct known-vm call lowering",
                 "Testing that direct child-function calls like labelFor(...) lower into the KNOWN_VM_CALL family instead of remaining on generic FUNCTION_CALL.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);

    sourceName = ZrCore_String_CreateFromNative(state, "direct_child_function_known_call_quickening_regression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL), 0) +
                    count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS), 0),
            "Direct child-function call sites should quicken to the KNOWN_VM_CALL family");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(FUNCTION_CALL), 0) +
                    count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS), 0),
            "Direct child-function call sites should not remain on the generic FUNCTION_CALL family");

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, function);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_loop_child_function_calls_quicken_to_known_vm_call_family(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Loop Child Function Calls Quicken To Known VM Call Family";
    const char *source =
            "labelFor(slot: int): string {\n"
            "    var normalized = slot % 4;\n"
            "    if (normalized == 0) {\n"
            "        return \"aa\";\n"
            "    }\n"
            "    if (normalized == 1) {\n"
            "        return \"bb\";\n"
            "    }\n"
            "    if (normalized == 2) {\n"
            "        return \"cc\";\n"
            "    }\n"
            "    return \"dd\";\n"
            "}\n"
            "var total = 0;\n"
            "var i = 0;\n"
            "while (i <= 3) {\n"
            "    var label = labelFor(i);\n"
            "    if (label == \"cc\") {\n"
            "        total = total + 1;\n"
            "    }\n"
            "    i = i + 1;\n"
            "}\n"
            "return total;\n";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;
    TZrInt64 result = 0;
    char genericCallWindow[1024];

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("loop known-vm call lowering",
                 "Testing that direct child-function calls inside loop bodies keep VM callable provenance and quicken to KNOWN_VM_CALL instead of remaining on generic FUNCTION_CALL.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);

    sourceName = ZrCore_String_CreateFromNative(state, "loop_child_function_known_call_quickening_regression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    build_opcode_window_message(function, ZR_INSTRUCTION_ENUM(FUNCTION_CALL), genericCallWindow, sizeof(genericCallWindow));

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL), 0),
            "Loop child-function call sites should quicken to KNOWN_VM_CALL");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(FUNCTION_CALL), 0) +
                    count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS), 0),
            genericCallWindow);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, function);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_matrix_add_2d_benchmark_project_compile_quickens_array_add_loop_calls(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Matrix Add 2D Benchmark Project Compile Quickens Array Add Loop Calls";
    ZrMatrixAdd2dCompileFixture fixture;
    TZrUInt32 totalGenericCallCount;
    TZrUInt32 totalKnownNativeCallCount;
    TZrUInt32 totalKnownNativeMemberCallCount;
    char firstGenericCallWindow[1024];
    char secondGenericCallWindow[1024];
    char thirdGenericCallWindow[1024];
    char fourthGenericCallWindow[1024];
    char functionCallHits[1024];
    char knownNativeCallHits[1024];
    char failureMessage[4096];

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("benchmark native member-call lowering",
                 "Testing that the real matrix_add_2d project compile path lowers the four hot Array.add(0) loop calls "
                 "into known native call opcodes instead of leaving them on generic FUNCTION_CALL.");

    TEST_ASSERT_TRUE_MESSAGE(ZrTests_PrepareMatrixAdd2dCompileFixture(&fixture, "known_native_array_add_loop_calls"),
                             "Failed to prepare fresh matrix_add_2d compile fixture");

    build_nth_opcode_window_message(fixture.function,
                                    ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                    1u,
                                    firstGenericCallWindow,
                                    sizeof(firstGenericCallWindow));
    build_nth_opcode_window_message(fixture.function,
                                    ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                    2u,
                                    secondGenericCallWindow,
                                    sizeof(secondGenericCallWindow));
    build_nth_opcode_window_message(fixture.function,
                                    ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                    3u,
                                    thirdGenericCallWindow,
                                    sizeof(thirdGenericCallWindow));
    build_nth_opcode_window_message(fixture.function,
                                    ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                    4u,
                                    fourthGenericCallWindow,
                                    sizeof(fourthGenericCallWindow));
    build_opcode_hits_message(fixture.function,
                              ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                              functionCallHits,
                              sizeof(functionCallHits));
    build_opcode_hits_message(fixture.function,
                              ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL),
                              knownNativeCallHits,
                              sizeof(knownNativeCallHits));

    totalGenericCallCount = count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(FUNCTION_CALL), 0);
    totalKnownNativeCallCount = count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL), 0);
    totalKnownNativeMemberCallCount =
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL), 0);

    snprintf(failureMessage,
             sizeof(failureMessage),
             "matrix_add_2d Array.add loop totals: generic=%u known-native=%u known-native-member=%u | function-call-hits: %s | "
             "known-native-call-hits: %s | first-generic: %s | second-generic: %s | third-generic: %s | "
             "fourth-generic: %s",
             (unsigned int)totalGenericCallCount,
             (unsigned int)totalKnownNativeCallCount,
             (unsigned int)totalKnownNativeMemberCallCount,
             functionCallHits,
             knownNativeCallHits,
             firstGenericCallWindow,
             secondGenericCallWindow,
             thirdGenericCallWindow,
             fourthGenericCallWindow);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, totalGenericCallCount, failureMessage);
    TEST_ASSERT_TRUE_MESSAGE(totalKnownNativeCallCount + totalKnownNativeMemberCallCount >= 6u, failureMessage);

    ZrTests_FreeMatrixAdd2dCompileFixture(&fixture);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_map_object_access_benchmark_project_compile_quickens_labelFor_loop_call(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Map Object Access Benchmark Project Compile Quickens LabelFor Loop Call";
    char projectPath[512];
    char sourcePath[512];
    char *source = ZR_NULL;
    SZrGlobalState *global = ZR_NULL;
    SZrState *state = ZR_NULL;
    SZrString *sourceName = ZR_NULL;
    SZrFunction *function = ZR_NULL;
    char genericCallWindow[1024];
    char knownCallWindow[1024];
    char firstGenericCallWindow[1024];
    char firstKnownCallWindow[1024];
    char secondGenericCallWindow[1024];
    char functionCallHits[1024];
    char superFunctionCallHits[1024];
    char knownVmCallHits[1024];
    char superKnownVmCallHits[1024];
    char labelForSlotSummary[1024];
    char secondGenericCalleeSummary[1024];
    char failureMessage[4096];
    const SZrFunctionLocalVariable *labelForLocal = ZR_NULL;
    const SZrFunctionExportedVariable *labelForExport = ZR_NULL;
    const SZrFunction *secondGenericOwner = ZR_NULL;
    TZrUInt32 secondGenericInstructionIndex = 0;
    TZrUInt32 secondGenericSeenCount = 0;
    TZrBool secondFunctionCallStillGeneric = ZR_FALSE;
    TZrUInt32 knownCallCount;
    TZrUInt32 genericCallCount;
    TZrUInt32 dynCallCount;
    TZrUInt32 addStringCount;
    TZrUInt32 totalKnownCallCount;
    TZrUInt32 totalGenericCallCount;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("benchmark loop known-vm call lowering",
                 "Testing that the real map_object_access project compile path quickens main.zr:33 labelFor(outer + inner) to KNOWN_VM_CALL instead of leaving it on generic FUNCTION_CALL.");

    snprintf(projectPath,
             sizeof(projectPath),
             "%s/benchmarks/cases/map_object_access/zr/benchmark_map_object_access.zrp",
             ZR_VM_TESTS_SOURCE_DIR);
    snprintf(sourcePath,
             sizeof(sourcePath),
             "%s/benchmarks/cases/map_object_access/zr/src/main.zr",
             ZR_VM_TESTS_SOURCE_DIR);

    global = ZrLibrary_CommonState_CommonGlobalState_New(projectPath);
    TEST_ASSERT_NOT_NULL(global);

    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(global));
    TEST_ASSERT_TRUE(ZrVmLibContainer_Register(global));
    TEST_ASSERT_NOT_NULL(state->global->compileSource);

    source = read_text_file_owned(sourcePath);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_CreateFromNative(state, sourcePath);
    TEST_ASSERT_NOT_NULL(sourceName);

    function = state->global->compileSource(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    build_opcode_window_message_for_source_line(function,
                                                ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                                33u,
                                                genericCallWindow,
                                                sizeof(genericCallWindow));
    build_opcode_window_message_for_source_line(function,
                                                ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL),
                                                33u,
                                                knownCallWindow,
                                                sizeof(knownCallWindow));
    build_opcode_window_message(function,
                                ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                firstGenericCallWindow,
                                sizeof(firstGenericCallWindow));
    build_opcode_window_message(function,
                                ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL),
                                firstKnownCallWindow,
                                sizeof(firstKnownCallWindow));
    build_nth_opcode_window_message(function,
                                    ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                    2u,
                                    secondGenericCallWindow,
                                    sizeof(secondGenericCallWindow));
    build_opcode_hits_message(function,
                              ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                              functionCallHits,
                              sizeof(functionCallHits));
    build_opcode_hits_message(function,
                              ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS),
                              superFunctionCallHits,
                              sizeof(superFunctionCallHits));
    build_opcode_hits_message(function,
                              ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL),
                              knownVmCallHits,
                              sizeof(knownVmCallHits));
    build_opcode_hits_message(function,
                              ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS),
                              superKnownVmCallHits,
                              sizeof(superKnownVmCallHits));
    secondGenericCalleeSummary[0] = '\0';
    if (find_nth_opcode_recursive(function,
                                  ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                  2u,
                                  0,
                                  &secondGenericOwner,
                                  &secondGenericInstructionIndex,
                                  &secondGenericSeenCount) &&
        secondGenericOwner != ZR_NULL) {
        secondFunctionCallStillGeneric = ZR_TRUE;
        size_t secondGenericCalleeSummaryLength = 0;
        const TZrInstruction *secondGenericInstruction =
                &secondGenericOwner->instructionsList[secondGenericInstructionIndex];
        append_slot_metadata(secondGenericCalleeSummary,
                             sizeof(secondGenericCalleeSummary),
                             &secondGenericCalleeSummaryLength,
                             secondGenericOwner,
                             secondGenericInstructionIndex,
                             (TZrUInt32)secondGenericInstruction->instruction.operand.operand1[0]);
    } else {
        snprintf(secondGenericCalleeSummary,
                 sizeof(secondGenericCalleeSummary),
                 "2nd FUNCTION_CALL callee summary unavailable");
    }
    labelForSlotSummary[0] = '\0';
    labelForLocal = find_local_variable_by_name(function, "labelFor");
    labelForExport = find_exported_variable_by_name(function, "labelFor");
    if (labelForLocal != ZR_NULL && function->instructionsLength > 0) {
        size_t labelSummaryLength = 0;
        append_slot_metadata(labelForSlotSummary,
                             sizeof(labelForSlotSummary),
                             &labelSummaryLength,
                             function,
                             function->instructionsLength - 1u,
                             labelForLocal->stackSlot);
    } else {
        snprintf(labelForSlotSummary, sizeof(labelForSlotSummary), "labelFor local binding not found");
    }

    knownCallCount = count_opcode_on_source_line_recursive(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL), 33u, 0);
    genericCallCount = count_opcode_on_source_line_recursive(function, ZR_INSTRUCTION_ENUM(FUNCTION_CALL), 33u, 0) +
                       count_opcode_on_source_line_recursive(function,
                                                             ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS),
                                                             33u,
                                                             0);
    dynCallCount = count_opcode_on_source_line_recursive(function, ZR_INSTRUCTION_ENUM(DYN_CALL), 33u, 0) +
                   count_opcode_on_source_line_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS), 33u, 0);
    addStringCount = count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_STRING), 0);
    totalKnownCallCount = count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL), 0) +
                          count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS), 0);
    totalGenericCallCount = count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(FUNCTION_CALL), 0) +
                            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS), 0);

    snprintf(failureMessage,
             sizeof(failureMessage),
             "line33 counts: KNOWN_VM_CALL=%u FUNCTION_CALL=%u DYN_CALL=%u | function counts: ADD_STRING=%u | total-known=%u total-generic=%u | line33-known-window: %s | line33-generic-window: %s | first-generic-window: %s | second-generic-window: %s | first-known-window: %s | function-call-hits: %s | super-function-call-hits: %s | known-vm-call-hits: %s | super-known-vm-call-hits: %s | second-generic-callee: %s",
             (unsigned int)knownCallCount,
             (unsigned int)genericCallCount,
             (unsigned int)dynCallCount,
             (unsigned int)addStringCount,
             (unsigned int)totalKnownCallCount,
             (unsigned int)totalGenericCallCount,
             knownCallWindow,
             genericCallWindow,
             firstGenericCallWindow,
             secondGenericCallWindow,
             firstKnownCallWindow,
             functionCallHits,
             superFunctionCallHits,
             knownVmCallHits,
             superKnownVmCallHits,
             secondGenericCalleeSummary);
    if (labelForLocal != ZR_NULL) {
        size_t usedLength = strlen(failureMessage);
        snprintf(failureMessage + usedLength,
                 sizeof(failureMessage) - usedLength,
                 " | labelFor-slot=%u export-child=%u export-kind=%u writers=%s",
                 (unsigned int)labelForLocal->stackSlot,
                 labelForExport != ZR_NULL ? (unsigned int)labelForExport->callableChildIndex : UINT32_MAX,
                 labelForExport != ZR_NULL ? (unsigned int)labelForExport->exportKind : UINT32_MAX,
                 labelForSlotSummary);
    }

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, genericCallCount, failureMessage);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, dynCallCount, failureMessage);
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0u, addStringCount, failureMessage);
    TEST_ASSERT_FALSE_MESSAGE(secondFunctionCallStillGeneric, failureMessage);
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0u, totalKnownCallCount, failureMessage);

    ZrCore_Function_Free(state, function);
    free(source);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    ZR_TEST_DIVIDER();
}

void test_call_chain_polymorphic_benchmark_project_compile_quickens_loop_helper_calls(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Call Chain Polymorphic Benchmark Project Compile Quickens Loop Helper Calls";
    ZrCallChainPolymorphicCompileFixture fixture;
    char dispatchLine79GenericWindow[1024];
    char dispatchLine81GenericWindow[1024];
    char dispatchLine83GenericWindow[1024];
    char tailLine86GenericWindow[1024];
    char recursiveTailLine59Window[1024];
    char functionCallHits[1024];
    char functionTailCallHits[1024];
    char knownVmCallHits[1024];
    char knownVmTailCallHits[1024];
    char fourthGenericCallCalleeSummary[1024];
    char fifthGenericCallCalleeSummary[1024];
    char firstGenericTailCalleeSummary[1024];
    char secondGenericTailCalleeSummary[1024];
    char thirdGenericTailCalleeSummary[1024];
    char fourthGenericTailCalleeSummary[1024];
    char genericOwnerInfo[512];
    char failureMessage[4096];
    const SZrFunction *genericOwner = ZR_NULL;
    const SZrFunction *genericTailOwner = ZR_NULL;
    TZrUInt32 genericInstructionIndex = 0;
    TZrUInt32 genericTailInstructionIndex = 0;
    TZrUInt32 seenCount = 0;
    TZrUInt32 line79GenericCallCount;
    TZrUInt32 line81GenericCallCount;
    TZrUInt32 line83GenericCallCount;
    TZrUInt32 line86GenericCallCount;
    TZrUInt32 line59GenericTailCallCount;
    TZrUInt32 totalGenericCallCount;
    TZrUInt32 totalGenericTailCallCount;
    TZrUInt32 totalKnownVmCallCount;
    TZrUInt32 totalKnownVmTailCallCount;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("benchmark polymorphic helper call lowering",
                 "Testing that the real call_chain_polymorphic project compile path quickens the loop helper calls "
                 "to the KNOWN_VM_CALL family instead of retaining generic FUNCTION_CALL / FUNCTION_TAIL_CALL "
                 "sites for dispatch(...) or tailAccumulate(...).");

    TEST_ASSERT_TRUE_MESSAGE(
            ZrTests_PrepareCallChainPolymorphicCompileFixture(&fixture, "known_vm_loop_helper_calls"),
            "Failed to prepare fresh call_chain_polymorphic compile fixture");

    build_opcode_window_message_for_source_line(fixture.function,
                                                ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                                79u,
                                                dispatchLine79GenericWindow,
                                                sizeof(dispatchLine79GenericWindow));
    build_opcode_window_message_for_source_line(fixture.function,
                                                ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                                81u,
                                                dispatchLine81GenericWindow,
                                                sizeof(dispatchLine81GenericWindow));
    build_opcode_window_message_for_source_line(fixture.function,
                                                ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                                83u,
                                                dispatchLine83GenericWindow,
                                                sizeof(dispatchLine83GenericWindow));
    build_opcode_window_message_for_source_line(fixture.function,
                                                ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                                86u,
                                                tailLine86GenericWindow,
                                                sizeof(tailLine86GenericWindow));
    build_opcode_window_message_for_source_line(fixture.function,
                                                ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL),
                                                59u,
                                                recursiveTailLine59Window,
                                                sizeof(recursiveTailLine59Window));
    build_opcode_hits_message(fixture.function,
                              ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                              functionCallHits,
                              sizeof(functionCallHits));
    build_opcode_hits_message(fixture.function,
                              ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL),
                              functionTailCallHits,
                              sizeof(functionTailCallHits));
    build_opcode_hits_message(fixture.function,
                              ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL),
                              knownVmCallHits,
                              sizeof(knownVmCallHits));
    build_opcode_hits_message(fixture.function,
                              ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL),
                              knownVmTailCallHits,
                              sizeof(knownVmTailCallHits));
    fourthGenericCallCalleeSummary[0] = '\0';
    fifthGenericCallCalleeSummary[0] = '\0';
    firstGenericTailCalleeSummary[0] = '\0';
    secondGenericTailCalleeSummary[0] = '\0';
    thirdGenericTailCalleeSummary[0] = '\0';
    fourthGenericTailCalleeSummary[0] = '\0';
    genericOwnerInfo[0] = '\0';
    seenCount = 0;
    if (find_nth_opcode_recursive(fixture.function,
                                  ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                  4u,
                                  0,
                                  &genericOwner,
                                  &genericInstructionIndex,
                                  &seenCount) &&
        genericOwner != ZR_NULL) {
        size_t summaryLength = 0;
        const TZrInstruction *instruction = &genericOwner->instructionsList[genericInstructionIndex];
        append_slot_metadata(fourthGenericCallCalleeSummary,
                             sizeof(fourthGenericCallCalleeSummary),
                             &summaryLength,
                             genericOwner,
                             genericInstructionIndex,
                             (TZrUInt32)instruction->instruction.operand.operand1[0]);
        snprintf(genericOwnerInfo,
                 sizeof(genericOwnerInfo),
                 "owner=%s ownerFunction=%s childCount=%u",
                 function_name_or_anonymous(genericOwner),
                 genericOwner->ownerFunction != ZR_NULL ? function_name_or_anonymous(genericOwner->ownerFunction)
                                                        : "<null>",
                 genericOwner->ownerFunction != ZR_NULL ? (unsigned int)genericOwner->ownerFunction->childFunctionLength
                                                        : 0u);
    }
    seenCount = 0;
    if (find_nth_opcode_recursive(fixture.function,
                                  ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                  5u,
                                  0,
                                  &genericOwner,
                                  &genericInstructionIndex,
                                  &seenCount) &&
        genericOwner != ZR_NULL) {
        size_t summaryLength = 0;
        const TZrInstruction *instruction = &genericOwner->instructionsList[genericInstructionIndex];
        append_slot_metadata(fifthGenericCallCalleeSummary,
                             sizeof(fifthGenericCallCalleeSummary),
                             &summaryLength,
                             genericOwner,
                             genericInstructionIndex,
                             (TZrUInt32)instruction->instruction.operand.operand1[0]);
    }
    seenCount = 0;
    if (find_nth_opcode_recursive(fixture.function,
                                  ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL),
                                  1u,
                                  0,
                                  &genericTailOwner,
                                  &genericTailInstructionIndex,
                                  &seenCount) &&
        genericTailOwner != ZR_NULL) {
        size_t summaryLength = 0;
        const TZrInstruction *instruction = &genericTailOwner->instructionsList[genericTailInstructionIndex];
        append_slot_metadata(firstGenericTailCalleeSummary,
                             sizeof(firstGenericTailCalleeSummary),
                             &summaryLength,
                             genericTailOwner,
                             genericTailInstructionIndex,
                             (TZrUInt32)instruction->instruction.operand.operand1[0]);
    }
    seenCount = 0;
    if (find_nth_opcode_recursive(fixture.function,
                                  ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL),
                                  2u,
                                  0,
                                  &genericTailOwner,
                                  &genericTailInstructionIndex,
                                  &seenCount) &&
        genericTailOwner != ZR_NULL) {
        size_t summaryLength = 0;
        const TZrInstruction *instruction = &genericTailOwner->instructionsList[genericTailInstructionIndex];
        append_slot_metadata(secondGenericTailCalleeSummary,
                             sizeof(secondGenericTailCalleeSummary),
                             &summaryLength,
                             genericTailOwner,
                             genericTailInstructionIndex,
                             (TZrUInt32)instruction->instruction.operand.operand1[0]);
    }
    seenCount = 0;
    if (find_nth_opcode_recursive(fixture.function,
                                  ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL),
                                  3u,
                                  0,
                                  &genericTailOwner,
                                  &genericTailInstructionIndex,
                                  &seenCount) &&
        genericTailOwner != ZR_NULL) {
        size_t summaryLength = 0;
        const TZrInstruction *instruction = &genericTailOwner->instructionsList[genericTailInstructionIndex];
        append_slot_metadata(thirdGenericTailCalleeSummary,
                             sizeof(thirdGenericTailCalleeSummary),
                             &summaryLength,
                             genericTailOwner,
                             genericTailInstructionIndex,
                             (TZrUInt32)instruction->instruction.operand.operand1[0]);
    }
    seenCount = 0;
    if (find_nth_opcode_recursive(fixture.function,
                                  ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL),
                                  4u,
                                  0,
                                  &genericTailOwner,
                                  &genericTailInstructionIndex,
                                  &seenCount) &&
        genericTailOwner != ZR_NULL) {
        size_t summaryLength = 0;
        const TZrInstruction *instruction = &genericTailOwner->instructionsList[genericTailInstructionIndex];
        append_slot_metadata(fourthGenericTailCalleeSummary,
                             sizeof(fourthGenericTailCalleeSummary),
                             &summaryLength,
                             genericTailOwner,
                             genericTailInstructionIndex,
                             (TZrUInt32)instruction->instruction.operand.operand1[0]);
    }

    line79GenericCallCount =
            count_opcode_on_source_line_recursive(fixture.function, ZR_INSTRUCTION_ENUM(FUNCTION_CALL), 79u, 0);
    line81GenericCallCount =
            count_opcode_on_source_line_recursive(fixture.function, ZR_INSTRUCTION_ENUM(FUNCTION_CALL), 81u, 0);
    line83GenericCallCount =
            count_opcode_on_source_line_recursive(fixture.function, ZR_INSTRUCTION_ENUM(FUNCTION_CALL), 83u, 0);
    line86GenericCallCount =
            count_opcode_on_source_line_recursive(fixture.function, ZR_INSTRUCTION_ENUM(FUNCTION_CALL), 86u, 0);
    line59GenericTailCallCount =
            count_opcode_on_source_line_recursive(fixture.function, ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL), 59u, 0);
    totalGenericCallCount = count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(FUNCTION_CALL), 0) +
                            count_opcode_recursive(
                                    fixture.function, ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS), 0);
    totalGenericTailCallCount = count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL), 0) +
                                count_opcode_recursive(
                                        fixture.function, ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS), 0);
    totalKnownVmCallCount = count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL), 0) +
                            count_opcode_recursive(
                                    fixture.function, ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS), 0);
    totalKnownVmTailCallCount =
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL), 0) +
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS), 0);

    snprintf(failureMessage,
             sizeof(failureMessage),
             "call_chain_polymorphic counts: line79-generic=%u line81-generic=%u line83-generic=%u "
             "line86-generic=%u line59-generic-tail=%u | totals generic=%u generic-tail=%u known=%u "
             "known-tail=%u | line79-window: %s | line81-window: %s | line83-window: %s | line86-window: %s "
             "| line59-tail-window: %s | function-call-hits: %s | function-tail-call-hits: %s | "
             "known-vm-call-hits: %s | known-vm-tail-call-hits: %s",
             (unsigned int)line79GenericCallCount,
             (unsigned int)line81GenericCallCount,
             (unsigned int)line83GenericCallCount,
             (unsigned int)line86GenericCallCount,
             (unsigned int)line59GenericTailCallCount,
             (unsigned int)totalGenericCallCount,
             (unsigned int)totalGenericTailCallCount,
             (unsigned int)totalKnownVmCallCount,
             (unsigned int)totalKnownVmTailCallCount,
             dispatchLine79GenericWindow,
             dispatchLine81GenericWindow,
             dispatchLine83GenericWindow,
             tailLine86GenericWindow,
             recursiveTailLine59Window,
             functionCallHits,
             functionTailCallHits,
             knownVmCallHits,
             knownVmTailCallHits);
    if (fourthGenericCallCalleeSummary[0] != '\0') {
        size_t usedLength = strlen(failureMessage);
        snprintf(failureMessage + usedLength,
                 sizeof(failureMessage) - usedLength,
                 " | 4th-generic-callee=%s",
                 fourthGenericCallCalleeSummary);
    }
    if (genericOwnerInfo[0] != '\0') {
        size_t usedLength = strlen(failureMessage);
        snprintf(failureMessage + usedLength,
                 sizeof(failureMessage) - usedLength,
                 " | generic-owner-info=%s",
                 genericOwnerInfo);
    }
    if (fifthGenericCallCalleeSummary[0] != '\0') {
        size_t usedLength = strlen(failureMessage);
        snprintf(failureMessage + usedLength,
                 sizeof(failureMessage) - usedLength,
                 " | 5th-generic-callee=%s",
                 fifthGenericCallCalleeSummary);
    }
    if (firstGenericTailCalleeSummary[0] != '\0') {
        size_t usedLength = strlen(failureMessage);
        snprintf(failureMessage + usedLength,
                 sizeof(failureMessage) - usedLength,
                 " | 1st-generic-tail-callee=%s",
                 firstGenericTailCalleeSummary);
    }
    if (secondGenericTailCalleeSummary[0] != '\0') {
        size_t usedLength = strlen(failureMessage);
        snprintf(failureMessage + usedLength,
                 sizeof(failureMessage) - usedLength,
                 " | 2nd-generic-tail-callee=%s",
                 secondGenericTailCalleeSummary);
    }
    if (thirdGenericTailCalleeSummary[0] != '\0') {
        size_t usedLength = strlen(failureMessage);
        snprintf(failureMessage + usedLength,
                 sizeof(failureMessage) - usedLength,
                 " | 3rd-generic-tail-callee=%s",
                 thirdGenericTailCalleeSummary);
    }
    if (fourthGenericTailCalleeSummary[0] != '\0') {
        size_t usedLength = strlen(failureMessage);
        snprintf(failureMessage + usedLength,
                 sizeof(failureMessage) - usedLength,
                 " | 4th-generic-tail-callee=%s",
                 fourthGenericTailCalleeSummary);
    }

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, line79GenericCallCount, failureMessage);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, line81GenericCallCount, failureMessage);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, line83GenericCallCount, failureMessage);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, line86GenericCallCount, failureMessage);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, line59GenericTailCallCount, failureMessage);
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0u, totalKnownVmCallCount, failureMessage);
    TEST_ASSERT_TRUE_MESSAGE(totalGenericCallCount <= 6u, failureMessage);
    TEST_ASSERT_TRUE_MESSAGE(totalGenericTailCallCount <= 4u, failureMessage);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_FreeCallChainPolymorphicCompileFixture(&fixture);
    ZR_TEST_DIVIDER();
}

void test_call_chain_polymorphic_dispatch_callable_parameter_quickens_to_known_vm_call(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Call Chain Polymorphic Dispatch Callable Parameter Quickens To Known VM Call";
    ZrCallChainPolymorphicCompileFixture fixture;
    const SZrFunction *dispatchFunction = ZR_NULL;
    TZrUInt32 knownVmTailCallCount;
    TZrUInt32 genericTailCallCount;
    TZrUInt32 dynTailCallCount;
    TZrUInt32 metaTailCallCount;
    char genericTailWindow[1024];
    char dynTailWindow[1024];
    char metaTailWindow[1024];
    char knownVmTailWindow[1024];
    char superDynTailCachedWindow[1024];
    char firstSuperDynTailCachedWindow[1024];
    char genericTailHits[1024];
    char dynTailHits[1024];
    char metaTailHits[1024];
    char knownVmTailHits[1024];
    char superDynTailCachedHits[1024];
    char line79KnownVmWindow[1024];
    char ownerCallsiteSlotSummary[1024];
    char secondKnownVmCallSlotSummary[1024];
    char firstKnownVmCallWindow[1024];
    char secondKnownVmCallWindow[1024];
    char thirdKnownVmCallWindow[1024];
    char fourthKnownVmCallWindow[1024];
    char rootKnownVmCallHits[1024];
    char failureMessage[4096];
    const SZrFunction *ownerKnownVmFunction = ZR_NULL;
    TZrUInt32 ownerKnownVmInstructionIndex = 0;
    const SZrFunction *secondKnownVmFunction = ZR_NULL;
    TZrUInt32 secondKnownVmInstructionIndex = 0;
    TZrUInt32 seenKnownVmCount = 0;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("benchmark dispatch callable-parameter provenance tail-call lowering",
                 "Testing that the real call_chain_polymorphic project compile path keeps VM callable provenance "
                 "inside dispatch(callable, ...) so the hot tail call site no longer stays on the generic "
                 "FUNCTION_TAIL_CALL / DYN_TAIL_CALL / META_TAIL_CALL families.");

    TEST_ASSERT_TRUE_MESSAGE(
            ZrTests_PrepareCallChainPolymorphicCompileFixture(&fixture, "dispatch_callable_parameter_known_vm_call"),
            "Failed to prepare fresh call_chain_polymorphic compile fixture");

    dispatchFunction = find_child_function_by_name_recursive(fixture.function, "dispatch", 0);
    TEST_ASSERT_NOT_NULL_MESSAGE(dispatchFunction, "call_chain_polymorphic fixture must retain child function dispatch");

    knownVmTailCallCount =
            count_opcode_recursive(dispatchFunction, ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL), 0) +
            count_opcode_recursive(dispatchFunction, ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS), 0);
    genericTailCallCount =
            count_opcode_recursive(dispatchFunction, ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL), 0) +
            count_opcode_recursive(dispatchFunction, ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS), 0);
    dynTailCallCount = count_opcode_recursive(dispatchFunction, ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL), 0) +
                       count_opcode_recursive(dispatchFunction, ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS), 0) +
                       count_opcode_recursive(dispatchFunction, ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED), 0);
    metaTailCallCount =
            count_opcode_recursive(dispatchFunction, ZR_INSTRUCTION_ENUM(META_TAIL_CALL), 0) +
            count_opcode_recursive(dispatchFunction, ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS), 0) +
            count_opcode_recursive(dispatchFunction, ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED), 0);

    build_opcode_window_message_for_source_line(dispatchFunction,
                                                ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL),
                                                63u,
                                                genericTailWindow,
                                                sizeof(genericTailWindow));
    build_opcode_window_message_for_source_line(dispatchFunction,
                                                ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL),
                                                63u,
                                                dynTailWindow,
                                                sizeof(dynTailWindow));
    build_opcode_window_message_for_source_line(dispatchFunction,
                                                ZR_INSTRUCTION_ENUM(META_TAIL_CALL),
                                                63u,
                                                metaTailWindow,
                                                sizeof(metaTailWindow));
    build_opcode_window_message_for_source_line(dispatchFunction,
                                                ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL),
                                                63u,
                                                knownVmTailWindow,
                                                sizeof(knownVmTailWindow));
    build_opcode_window_message_for_source_line(dispatchFunction,
                                                ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED),
                                                63u,
                                                superDynTailCachedWindow,
                                                sizeof(superDynTailCachedWindow));
    build_nth_opcode_window_message(dispatchFunction,
                                    ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED),
                                    1u,
                                    firstSuperDynTailCachedWindow,
                                    sizeof(firstSuperDynTailCachedWindow));
    build_opcode_window_message_for_source_line(fixture.function,
                                                ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL),
                                                79u,
                                                line79KnownVmWindow,
                                                sizeof(line79KnownVmWindow));
    build_nth_opcode_window_message(fixture.function,
                                    ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL),
                                    1u,
                                    firstKnownVmCallWindow,
                                    sizeof(firstKnownVmCallWindow));
    build_nth_opcode_window_message(fixture.function,
                                    ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL),
                                    2u,
                                    secondKnownVmCallWindow,
                                    sizeof(secondKnownVmCallWindow));
    build_nth_opcode_window_message(fixture.function,
                                    ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL),
                                    3u,
                                    thirdKnownVmCallWindow,
                                    sizeof(thirdKnownVmCallWindow));
    build_nth_opcode_window_message(fixture.function,
                                    ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL),
                                    4u,
                                    fourthKnownVmCallWindow,
                                    sizeof(fourthKnownVmCallWindow));
    build_opcode_hits_message(dispatchFunction,
                              ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL),
                              genericTailHits,
                              sizeof(genericTailHits));
    build_opcode_hits_message(dispatchFunction,
                              ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL),
                              dynTailHits,
                              sizeof(dynTailHits));
    build_opcode_hits_message(dispatchFunction,
                              ZR_INSTRUCTION_ENUM(META_TAIL_CALL),
                              metaTailHits,
                              sizeof(metaTailHits));
    build_opcode_hits_message(dispatchFunction,
                              ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL),
                              knownVmTailHits,
                              sizeof(knownVmTailHits));
    build_opcode_hits_message(dispatchFunction,
                              ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED),
                              superDynTailCachedHits,
                              sizeof(superDynTailCachedHits));
    build_opcode_hits_message(fixture.function,
                              ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL),
                              rootKnownVmCallHits,
                              sizeof(rootKnownVmCallHits));
    ownerCallsiteSlotSummary[0] = '\0';
    if (find_first_opcode_on_source_line_recursive(fixture.function,
                                                   ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL),
                                                   79u,
                                                   0,
                                                   &ownerKnownVmFunction,
                                                   &ownerKnownVmInstructionIndex) &&
        ownerKnownVmFunction != ZR_NULL) {
        const TZrInstruction *ownerKnownVmInstruction =
                &ownerKnownVmFunction->instructionsList[ownerKnownVmInstructionIndex];
        size_t ownerSummaryLength = 0;
        TZrUInt32 calleeSlot = (TZrUInt32)ownerKnownVmInstruction->instruction.operand.operand1[0];
        append_slot_metadata(ownerCallsiteSlotSummary,
                             sizeof(ownerCallsiteSlotSummary),
                             &ownerSummaryLength,
                             ownerKnownVmFunction,
                             ownerKnownVmInstructionIndex,
                             calleeSlot);
        append_slot_metadata(ownerCallsiteSlotSummary,
                             sizeof(ownerCallsiteSlotSummary),
                             &ownerSummaryLength,
                             ownerKnownVmFunction,
                             ownerKnownVmInstructionIndex,
                             calleeSlot + 1u);
    }
    secondKnownVmCallSlotSummary[0] = '\0';
    seenKnownVmCount = 0;
    if (find_nth_opcode_recursive(fixture.function,
                                  ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL),
                                  2u,
                                  0,
                                  &secondKnownVmFunction,
                                  &secondKnownVmInstructionIndex,
                                  &seenKnownVmCount) &&
        secondKnownVmFunction != ZR_NULL) {
        const TZrInstruction *secondKnownVmInstruction =
                &secondKnownVmFunction->instructionsList[secondKnownVmInstructionIndex];
        size_t secondSummaryLength = 0;
        TZrUInt32 calleeSlot = (TZrUInt32)secondKnownVmInstruction->instruction.operand.operand1[0];
        TZrUInt32 callableTempSlot = calleeSlot + 1u;
        append_slot_metadata(secondKnownVmCallSlotSummary,
                             sizeof(secondKnownVmCallSlotSummary),
                             &secondSummaryLength,
                             secondKnownVmFunction,
                             secondKnownVmInstructionIndex,
                             calleeSlot);
        append_slot_metadata(secondKnownVmCallSlotSummary,
                             sizeof(secondKnownVmCallSlotSummary),
                             &secondSummaryLength,
                             secondKnownVmFunction,
                             secondKnownVmInstructionIndex,
                             callableTempSlot);
        if (callableTempSlot < secondKnownVmInstruction->instruction.operandExtra &&
            callableTempSlot < secondKnownVmFunction->instructionsLength) {
            /* no-op guard; source slot is recovered from the GET_STACK writer below */
        }
        if (secondKnownVmInstructionIndex > 0) {
            const TZrInstruction *callableTempWriter =
                    &secondKnownVmFunction->instructionsList[secondKnownVmInstructionIndex - 1u];
            if ((EZrInstructionCode)callableTempWriter->instruction.operationCode == ZR_INSTRUCTION_ENUM(GET_STACK) &&
                callableTempWriter->instruction.operandExtra == callableTempSlot) {
                append_slot_metadata(secondKnownVmCallSlotSummary,
                                     sizeof(secondKnownVmCallSlotSummary),
                                     &secondSummaryLength,
                                     secondKnownVmFunction,
                                     secondKnownVmInstructionIndex,
                                     (TZrUInt32)callableTempWriter->instruction.operand.operand2[0]);
            }
        }
    }

    snprintf(failureMessage,
             sizeof(failureMessage),
             "dispatch child tail-call counts: known-vm=%u generic=%u dyn=%u meta=%u | known-vm-hits: %s | "
             "generic-hits: %s | dyn-hits: %s | meta-hits: %s | super-dyn-tail-cached-hits: %s | "
             "line63-known-vm: %s | line63-generic: %s | line63-dyn: %s | line63-meta: %s | "
             "line63-super-dyn-tail-cached: %s | first-super-dyn-tail-cached: %s | "
             "line79-known-vm-window: %s | owner-callsite-slots: %s | root-known-vm-hits: %s | "
             "known-vm#1: %s | known-vm#2: %s | known-vm#3: %s | known-vm#4: %s | "
             "known-vm#2-slots: %s",
             (unsigned int)knownVmTailCallCount,
             (unsigned int)genericTailCallCount,
             (unsigned int)dynTailCallCount,
             (unsigned int)metaTailCallCount,
             knownVmTailHits,
             genericTailHits,
             dynTailHits,
             metaTailHits,
             superDynTailCachedHits,
             knownVmTailWindow,
             genericTailWindow,
             dynTailWindow,
             metaTailWindow,
             superDynTailCachedWindow,
             firstSuperDynTailCachedWindow,
             line79KnownVmWindow,
             ownerCallsiteSlotSummary,
             rootKnownVmCallHits,
             firstKnownVmCallWindow,
             secondKnownVmCallWindow,
             thirdKnownVmCallWindow,
             fourthKnownVmCallWindow,
             secondKnownVmCallSlotSummary);

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0u, knownVmTailCallCount, failureMessage);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, genericTailCallCount, failureMessage);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, dynTailCallCount, failureMessage);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, metaTailCallCount, failureMessage);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_FreeCallChainPolymorphicCompileFixture(&fixture);
    ZR_TEST_DIVIDER();
}

void test_repeated_constructor_string_arguments_survive_quickening_across_calls(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Repeated Constructor String Arguments Survive Quickening Across Calls";
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var {Array, Pair} = %import(\"zr.container\");\n"
            "var xs = new container.Array<Pair<int, string>>();\n"
            "xs.add(new container.Pair<int, string>(2, \"even\"));\n"
            "xs.add(new container.Pair<int, string>(4, \"even\"));\n"
            "return 0;\n";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;
    TZrInt32 evenConstantIndex;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Quickening repeated constant reloads across call boundaries",
                 "Testing that quickening does not erase a repeated string GET_CONSTANT when the first load was consumed by an earlier call frame");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmLibContainer_Register(state->global));

    sourceName = ZrCore_String_CreateFromNative(state, "constructor_string_reload_quickening_regression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    evenConstantIndex = find_top_level_string_constant_index(function, "even");
    TEST_ASSERT_TRUE_MESSAGE(evenConstantIndex >= 0, "Expected top-level constant pool to contain string literal 'even'");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            2u,
            count_get_constant_uses_recursive(function, (TZrUInt32)evenConstantIndex, 0),
            "Quickening should preserve both constructor argument loads for repeated string literals across calls");

    ZrCore_Function_Free(state, function);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_initializer_bound_local_is_visible_on_next_source_line(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Initializer Bound Local Is Visible On Next Source Line";
    const char *source =
            "var first = 1;\n"
            "var second = first + 2;\n"
            "return second;\n";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;
    const SZrFunctionLocalVariable *secondLocal;
    TZrUInt32 returnLineInstructionIndex;
    SZrString *visibleName;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Initializer local live range",
                 "Testing that a local bound to a reserved initializer slot is already visible to debug/local metadata at the first instruction of the next source line.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);

    sourceName = ZrCore_String_CreateFromNative(state, "initializer_local_visibility_regression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    secondLocal = find_local_variable_by_name(function, "second");
    TEST_ASSERT_NOT_NULL(secondLocal);
    returnLineInstructionIndex = find_first_execution_location_offset_for_line(function, 3u);
    TEST_ASSERT_NOT_EQUAL_UINT32_MESSAGE(UINT32_MAX,
                                         returnLineInstructionIndex,
                                         "Expected compiled function to record a debugger execution location for line 3");

    visibleName = ZrCore_Function_GetLocalVariableName(function, secondLocal->stackSlot, returnLineInstructionIndex);
    TEST_ASSERT_NOT_NULL_MESSAGE(
            visibleName,
            "Initializer-bound local 'second' should already be visible at the first instruction of the next source line");
    TEST_ASSERT_EQUAL_STRING("second", ZrCore_String_GetNativeString(visibleName));

    ZrCore_Function_Free(state, function);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_noop_primitive_casts_do_not_emit_conversion_opcodes(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "No-op Primitive Casts Do Not Emit Conversion Opcodes";
    const char *source =
            "id(value: int): int {\n"
            "    return <int> value;\n"
            "}\n"
            "sameFlag(flag: bool): bool {\n"
            "    return <bool> flag;\n"
            "}\n"
            "return id(7) + (sameFlag(true) ? 1 : 0);\n";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;
    SZrTypeValue result;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("No-op primitive cast quickening",
                 "Testing that primitive casts whose source type already matches the target type reuse the source slot.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);

    sourceName = ZrCore_String_CreateFromNative(state, "noop_primitive_cast_compile_regression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u,
                                     count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_INT), 0),
                                     "A cast from int to int should not materialize a TO_INT opcode");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u,
                                     count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_BOOL), 0),
                                     "A cast from bool to bool should not materialize a TO_BOOL opcode");

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(8, result.value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}

void test_logical_short_circuit_runtime_preserves_side_effect_boundaries(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Logical Short Circuit Runtime Preserves Side Effect Boundaries";
    const char *source =
            "var counter = 0;\n"
            "var touch = () => {\n"
            "    counter = counter + 1;\n"
            "    return true;\n"
            "};\n"
            "if (!(false || touch())) { return -1; }\n"
            "if (counter != 1) { return -2; }\n"
            "if (!(true || touch())) { return -3; }\n"
            "if (counter != 1) { return -4; }\n"
            "if (false && touch()) { return -5; }\n"
            "if (counter != 1) { return -6; }\n"
            "if (!(true && touch())) { return -7; }\n"
            "if (counter != 2) { return -8; }\n"
            "return 1;\n";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;
    SZrTypeValue result;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Logical short-circuit runtime",
                 "Testing that || and && evaluate the right operand only on the correct runtime paths while preserving the expected result value.");

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);

    sourceName = ZrCore_String_CreateFromNative(state, "logical_short_circuit_runtime_regression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(1, result.value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrTests_Runtime_State_Destroy(state);
    ZR_TEST_DIVIDER();
}
