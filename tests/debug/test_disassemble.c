#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser.h"

static SZrFunction *compile_source(SZrState *state, const char *source, const char *sourceLabel) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(sourceLabel);

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceLabel, strlen(sourceLabel));
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *find_child_function_by_name(SZrFunction *function, const char *name) {
    TZrUInt32 index;

    if (function == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    if (function->functionName != ZR_NULL) {
        const char *functionName = ZrCore_String_GetNativeString(function->functionName);
        if (functionName != ZR_NULL && strcmp(functionName, name) == 0) {
            return function;
        }
    }

    for (index = 0u; index < function->childFunctionLength; index++) {
        SZrFunction *match = find_child_function_by_name(&function->childFunctionList[index], name);
        if (match != ZR_NULL) {
            return match;
        }
    }

    return ZR_NULL;
}

static TZrSize read_file_into_buffer(FILE *file, char *buffer, TZrSize bufferSize) {
    size_t readCount;

    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(bufferSize > 0u);

    rewind(file);
    readCount = fread(buffer, 1, (size_t)bufferSize - 1u, file);
    buffer[readCount] = '\0';
    return (TZrSize)readCount;
}

static TZrSize count_instruction_rows(const char *text) {
    TZrSize count = 0u;
    const char *cursor = text;

    while (cursor != ZR_NULL && *cursor != '\0') {
        if (isdigit((unsigned char)cursor[0]) &&
            isdigit((unsigned char)cursor[1]) &&
            isdigit((unsigned char)cursor[2]) &&
            isdigit((unsigned char)cursor[3]) &&
            cursor[4] == ' ') {
            count++;
        }

        cursor = strchr(cursor, '\n');
        if (cursor != ZR_NULL) {
            cursor++;
        }
    }

    return count;
}

static void test_disassemble_function_prints_opcode_count_and_line_comments(void) {
    const char *source =
            "func add(left: int, right: int): int {\n"
            "    return left + right;\n"
            "}\n"
            "var result = add(1, 2);\n"
            "return result;\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *entryFunction;
    SZrFunction *addFunction;
    FILE *output;
    char buffer[8192];
    TZrSize written;

    TEST_ASSERT_NOT_NULL(state);
    entryFunction = compile_source(state, source, "disassemble_test.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    addFunction = find_child_function_by_name(entryFunction, "add");
    TEST_ASSERT_NOT_NULL(addFunction);

    output = tmpfile();
    TEST_ASSERT_NOT_NULL(output);
    ZrCore_Debug_DisassembleFunction(state, addFunction, output);
    written = read_file_into_buffer(output, buffer, sizeof(buffer));
    fclose(output);

    TEST_ASSERT_TRUE(written > 0u);
    TEST_ASSERT_NOT_NULL(strstr(buffer, "ZR_DISASSEMBLY function add"));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "offset opcode operands"));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "FUNCTION_RETURN"));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "; line "));
    TEST_ASSERT_EQUAL_UINT32(addFunction->instructionsLength, count_instruction_rows(buffer));

    ZrCore_Function_Free(state, entryFunction);
    ZrTests_Runtime_State_Destroy(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_disassemble_function_prints_opcode_count_and_line_comments);
    return UNITY_END();
}
