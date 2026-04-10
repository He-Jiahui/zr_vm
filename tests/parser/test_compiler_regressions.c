#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "unity.h"

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

void setUp(void) {}

void tearDown(void) {}

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
                arithmeticOpcode != ZR_INSTRUCTION_ENUM(ADD_INT_CONST) ||
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

static void test_class_member_nested_functions_keep_constant_indices_in_range(void) {
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

static void test_lambda_create_closure_targets_are_reachable_from_child_function_graph(void) {
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

static void test_classes_full_module_compiles_without_static_and_receiver_signature_regressions(void) {
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

static void test_native_network_optional_argument_import_compiles_without_unknown_parameter_blowup(void) {
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

static void test_reserved_type_query_targets_compile_without_explicit_imports(void) {
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

static void test_qualified_container_types_compile_through_function_predeclaration_paths(void) {
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

static void test_native_network_loopback_runtime_returns_expected_payload(void) {
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

static void test_native_network_loopback_project_run_returns_expected_payload(void) {
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

static void test_imported_source_module_type_stubs_do_not_serialize_into_entry_prototype_data(void) {
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

static void test_project_local_struct_pair_shadows_native_pair_at_runtime(void) {
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

static void test_lsp_language_feature_matrix_runtime_returns_expected_total(void) {
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

static void test_lsp_language_feature_matrix_copy_runtime_keeps_top_level_closure_captures_stable(void) {
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

static void test_benchmark_numeric_loops_project_run_returns_expected_checksum(void) {
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

static void test_matrix_add_2d_compile_avoids_adjacent_temp_reloads_before_super_array_int_ops(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Matrix Add 2D Compile Avoids Adjacent Temp Reloads Before Super Array Int Ops";
    char projectPath[512];
    char sourcePath[512];
    char *source = ZR_NULL;
    SZrGlobalState *global = ZR_NULL;
    SZrState *state = ZR_NULL;
    SZrString *sourceName;
    SZrFunction *function;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Compiler temp-forwarding peephole",
                 "Testing that matrix_add_2d no longer emits adjacent GET_STACK temp reloads directly feeding SUPER_ARRAY_GET_INT and SUPER_ARRAY_SET_INT");

    snprintf(projectPath,
             sizeof(projectPath),
             "%s/benchmarks/cases/matrix_add_2d/zr/benchmark_matrix_add_2d.zrp",
             ZR_VM_TESTS_SOURCE_DIR);
    snprintf(sourcePath,
             sizeof(sourcePath),
             "%s/benchmarks/cases/matrix_add_2d/zr/src/main.zr",
             ZR_VM_TESTS_SOURCE_DIR);

    global = ZrLibrary_CommonState_CommonGlobalState_New(projectPath);
    TEST_ASSERT_NOT_NULL(global);

    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);

    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE(ZrVmLibContainer_Register(global));
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(global));

    source = read_text_file_owned(sourcePath);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_CreateFromNative(state, sourcePath);
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    assert_super_array_int_ops_do_not_reload_adjacent_temp_slots(function, 0);
    TEST_ASSERT_TRUE_MESSAGE(
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST), 0) > 0,
            "matrix_add_2d should quicken the zero-fill loop into SUPER_ARRAY_FILL_INT4_CONST");

    free(source);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    ZR_TEST_DIVIDER();
}

static void test_matrix_add_2d_compile_folds_right_hand_int_constants_into_const_opcodes(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Matrix Add 2D Compile Folds Right Hand Int Constants Into Const Opcodes";
    char projectPath[512];
    char sourcePath[512];
    char *source = ZR_NULL;
    SZrGlobalState *global = ZR_NULL;
    SZrState *state = ZR_NULL;
    SZrString *sourceName;
    SZrFunction *function;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Const arithmetic quickening",
                 "Testing that matrix_add_2d lowers hot GET_CONSTANT plus int arithmetic chains into dedicated *_CONST opcodes instead of keeping the generic two-instruction form.");

    snprintf(projectPath,
             sizeof(projectPath),
             "%s/benchmarks/cases/matrix_add_2d/zr/benchmark_matrix_add_2d.zrp",
             ZR_VM_TESTS_SOURCE_DIR);
    snprintf(sourcePath,
             sizeof(sourcePath),
             "%s/benchmarks/cases/matrix_add_2d/zr/src/main.zr",
             ZR_VM_TESTS_SOURCE_DIR);

    global = ZrLibrary_CommonState_CommonGlobalState_New(projectPath);
    TEST_ASSERT_NOT_NULL(global);

    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);

    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE(ZrVmLibContainer_Register(global));
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(global));

    source = read_text_file_owned(sourcePath);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_CreateFromNative(state, sourcePath);
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST), 0),
            "matrix_add_2d should quicken right-hand constant multiplies into MUL_SIGNED_CONST");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_INT_CONST), 0),
            "matrix_add_2d should quicken right-hand constant additions into ADD_INT_CONST");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST), 0),
            "matrix_add_2d should quicken right-hand constant mod operations into MOD_SIGNED_CONST");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST), 0),
            "matrix_add_2d should quicken right-hand constant divisions into DIV_SIGNED_CONST");

    ZrCore_Function_Free(state, function);
    free(source);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    ZR_TEST_DIVIDER();
}

static void test_matrix_add_2d_compile_eliminates_temp_self_updates_for_add_int_const(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Matrix Add 2D Compile Eliminates Temp Self Updates For Add Int Const";
    char projectPath[512];
    char sourcePath[512];
    char *source = ZR_NULL;
    SZrGlobalState *global = ZR_NULL;
    SZrState *state = ZR_NULL;
    SZrString *sourceName;
    SZrFunction *function;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Loop self-update peephole",
                 "Testing that matrix_add_2d no longer keeps GET_STACK plus ADD_INT_CONST plus SET_STACK temp chains when the updated value is written back to the same slot.");

    snprintf(projectPath,
             sizeof(projectPath),
             "%s/benchmarks/cases/matrix_add_2d/zr/benchmark_matrix_add_2d.zrp",
             ZR_VM_TESTS_SOURCE_DIR);
    snprintf(sourcePath,
             sizeof(sourcePath),
             "%s/benchmarks/cases/matrix_add_2d/zr/src/main.zr",
             ZR_VM_TESTS_SOURCE_DIR);

    global = ZrLibrary_CommonState_CommonGlobalState_New(projectPath);
    TEST_ASSERT_NOT_NULL(global);

    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);

    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE(ZrVmLibContainer_Register(global));
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(global));

    source = read_text_file_owned(sourcePath);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_CreateFromNative(state, sourcePath);
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            0u,
            count_stack_self_update_int_const_triplets_recursive(function, 0),
            "matrix_add_2d should fold ADD_INT_CONST self-updates into direct destination writes");

    ZrCore_Function_Free(state, function);
    free(source);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    ZR_TEST_DIVIDER();
}

static void test_matrix_add_2d_compile_eliminates_generic_array_int_index_opcodes(void) {
    SZrRegressionTestTimer timer;
    const TZrChar *testSummary = "Matrix Add 2D Compile Eliminates Generic Array Int Index Opcodes";
    char projectPath[512];
    char sourcePath[512];
    char diagnostic[2048];
    char *source = ZR_NULL;
    SZrGlobalState *global = ZR_NULL;
    SZrState *state = ZR_NULL;
    SZrString *sourceName;
    SZrFunction *function;
    TZrUInt32 genericGetCount;
    TZrUInt32 genericSetCount;
    TZrUInt32 fastGetCount;
    TZrUInt32 fastSetCount;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Array<int> index quickening coverage",
                 "Testing that matrix_add_2d no longer leaves generic GET_BY_INDEX/SET_BY_INDEX opcodes for its typed Array<int> hot paths.");

    snprintf(projectPath,
             sizeof(projectPath),
             "%s/benchmarks/cases/matrix_add_2d/zr/benchmark_matrix_add_2d.zrp",
             ZR_VM_TESTS_SOURCE_DIR);
    snprintf(sourcePath,
             sizeof(sourcePath),
             "%s/benchmarks/cases/matrix_add_2d/zr/src/main.zr",
             ZR_VM_TESTS_SOURCE_DIR);

    global = ZrLibrary_CommonState_CommonGlobalState_New(projectPath);
    TEST_ASSERT_NOT_NULL(global);

    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);

    ZrParser_ToGlobalState_Register(state);
    TEST_ASSERT_TRUE(ZrVmLibContainer_Register(global));
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(global));

    source = read_text_file_owned(sourcePath);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_CreateFromNative(state, sourcePath);
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    genericGetCount = count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(GET_BY_INDEX), 0);
    genericSetCount = count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SET_BY_INDEX), 0);
    fastGetCount = count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT), 0) +
                   count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST), 0);
    fastSetCount = count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT), 0);

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            fastGetCount,
            "matrix_add_2d should emit SUPER_ARRAY_GET_INT-family opcodes for typed Array<int> reads");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u,
            fastSetCount,
            "matrix_add_2d should emit SUPER_ARRAY_SET_INT for typed Array<int> writes");
    if (genericGetCount != 0) {
        build_opcode_window_message(function, ZR_INSTRUCTION_ENUM(GET_BY_INDEX), diagnostic, sizeof(diagnostic));
        TEST_FAIL_MESSAGE(diagnostic);
    }
    if (genericSetCount != 0) {
        build_opcode_window_message(function, ZR_INSTRUCTION_ENUM(SET_BY_INDEX), diagnostic, sizeof(diagnostic));
        TEST_FAIL_MESSAGE(diagnostic);
    }

    ZrCore_Function_Free(state, function);
    free(source);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    ZR_TEST_DIVIDER();
}

static void test_repeated_constructor_string_arguments_survive_quickening_across_calls(void) {
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

int main(void) {
    printf("\n");
    ZR_TEST_MODULE_DIVIDER();
    printf("ZR-VM Compiler Regression Unit Tests\n");
    ZR_TEST_MODULE_DIVIDER();
    printf("\n");

    UNITY_BEGIN();
    RUN_TEST(test_class_member_nested_functions_keep_constant_indices_in_range);
    RUN_TEST(test_lambda_create_closure_targets_are_reachable_from_child_function_graph);
    RUN_TEST(test_classes_full_module_compiles_without_static_and_receiver_signature_regressions);
    RUN_TEST(test_native_network_optional_argument_import_compiles_without_unknown_parameter_blowup);
    RUN_TEST(test_reserved_type_query_targets_compile_without_explicit_imports);
    RUN_TEST(test_qualified_container_types_compile_through_function_predeclaration_paths);
    RUN_TEST(test_native_network_loopback_runtime_returns_expected_payload);
    RUN_TEST(test_native_network_loopback_project_run_returns_expected_payload);
    RUN_TEST(test_imported_source_module_type_stubs_do_not_serialize_into_entry_prototype_data);
    RUN_TEST(test_project_local_struct_pair_shadows_native_pair_at_runtime);
    RUN_TEST(test_lsp_language_feature_matrix_runtime_returns_expected_total);
    RUN_TEST(test_lsp_language_feature_matrix_copy_runtime_keeps_top_level_closure_captures_stable);
    RUN_TEST(test_benchmark_numeric_loops_project_run_returns_expected_checksum);
    RUN_TEST(test_matrix_add_2d_compile_avoids_adjacent_temp_reloads_before_super_array_int_ops);
    RUN_TEST(test_matrix_add_2d_compile_folds_right_hand_int_constants_into_const_opcodes);
    RUN_TEST(test_matrix_add_2d_compile_eliminates_temp_self_updates_for_add_int_const);
    RUN_TEST(test_matrix_add_2d_compile_eliminates_generic_array_int_index_opcodes);
    RUN_TEST(test_repeated_constructor_string_arguments_survive_quickening_across_calls);
    return UNITY_END();
}
