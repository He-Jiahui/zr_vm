#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "unity.h"

#include "matrix_add_2d_compile_fixture.h"
#include "runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/task_runtime.h"
#include "zr_vm_core/value.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_ffi/module.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_network/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_library/common_state.h"
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
                written = snprintf(buffer + *length,
                                   bufferSize - *length,
                                   " writer#%u=%s",
                                   (unsigned int)index,
                                   instruction_opcode_name((EZrInstructionCode)instruction->instruction.operationCode));
                if (written <= 0 || (size_t)written >= bufferSize - *length) {
                    *length = bufferSize;
                    return;
                }
                *length += (size_t)written;
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

static TZrBool opcode_is_super_array_get_int_family(EZrInstructionCode opcode) {
    return opcode == ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT) ||
           opcode == ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST);
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
        } else if (opcode == ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT)) {
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
                                                                       : "SUPER_ARRAY_SET_INT",
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
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT):
            readsSlot = instruction->instruction.operand.operand1[0] == oldSlot ||
                        instruction->instruction.operand.operand1[1] == oldSlot;
            break;
        case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT):
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
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT):
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
            count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST), 0);
    fastGetCount = count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT), 0) +
                   plainFastGetCount;
    fastSetCount = count_opcode_recursive(fixture.function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT), 0);

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            fastGetCount,
            "matrix_add_2d should emit SUPER_ARRAY_GET_INT-family opcodes for typed Array<int> reads");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            fastSetCount,
            "matrix_add_2d should emit SUPER_ARRAY_SET_INT for typed Array<int> writes");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            plainFastGetCount,
            "matrix_add_2d should emit SUPER_ARRAY_GET_INT_PLAIN_DEST for plain int temporaries on its hot read path");
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
