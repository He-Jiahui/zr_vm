#include <stdio.h>
#include <string.h>

#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"

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

static void test_heap_summary_prints_object_counts_bytes_and_gc_stats(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *stringValue;
    SZrObject *objectValue;
    FILE *output;
    char buffer[8192];
    TZrSize written;

    TEST_ASSERT_NOT_NULL(state);

    stringValue = ZrCore_String_CreateFromNative(state, "heap-summary");
    TEST_ASSERT_NOT_NULL(stringValue);
    objectValue = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(objectValue);
    ZrCore_Object_Init(state, objectValue);

    output = tmpfile();
    TEST_ASSERT_NOT_NULL(output);
    ZrCore_Debug_HeapSummary(state, output);
    written = read_file_into_buffer(output, buffer, sizeof(buffer));
    fclose(output);

    TEST_ASSERT_TRUE(written > 0u);
    TEST_ASSERT_NOT_NULL(strstr(buffer, "ZR_HEAP_SUMMARY objects "));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "type string count "));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "type object count "));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "type thread count "));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "gc regions "));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "gc collections "));

    ZrTests_Runtime_State_Destroy(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_heap_summary_prints_object_counts_bytes_and_gc_stats);
    return UNITY_END();
}
