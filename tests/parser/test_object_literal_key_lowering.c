#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser.h"

void setUp(void) {}

void tearDown(void) {}

static TZrPtr test_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        if (pointer != ZR_NULL && (TZrPtr)pointer >= (TZrPtr)0x1000) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }

    if ((TZrPtr)pointer >= (TZrPtr)0x1000) {
        return realloc(pointer, newSize);
    }

    return malloc(newSize);
}

static SZrState *create_test_state(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 0, &callbacks);

    if (global == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_GlobalState_InitRegistry(global->mainThreadState, global);
    return global->mainThreadState;
}

static void destroy_test_state(SZrState *state) {
    if (state != ZR_NULL && state->global != ZR_NULL) {
        ZrCore_GlobalState_Free(state->global);
    }
}

static TZrUInt32 function_count_opcode(const SZrFunction *function, EZrInstructionCode opcode) {
    TZrUInt32 count = 0;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return 0;
    }

    for (TZrUInt32 i = 0; i < function->instructionsLength; i++) {
        if ((EZrInstructionCode) function->instructionsList[i].instruction.operationCode == opcode) {
            count++;
        }
    }

    return count;
}

static void test_computed_identifier_object_key_emits_index_set(void) {
    const char *source =
            "var key = \"name\";\n"
            "var obj = {[key]: 1, name: 2};";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);

    sourceName = ZrCore_String_Create(state, "computed_object_key_opcode_test.zr", 34);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);

    function = ZrParser_Compiler_Compile(state, ast);
    ZrParser_Ast_Free(state, ast);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_EQUAL_UINT32(1, function_count_opcode(function, ZR_INSTRUCTION_ENUM(SET_BY_INDEX)));
    TEST_ASSERT_EQUAL_UINT32(1, function_count_opcode(function, ZR_INSTRUCTION_ENUM(SET_MEMBER)));

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_computed_identifier_object_key_emits_index_set);
    return UNITY_END();
}
