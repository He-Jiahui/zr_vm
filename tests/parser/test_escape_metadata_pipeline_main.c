#include <stdio.h>

#include "unity.h"
#include "zr_test_log_macros.h"

extern void test_compiler_escape_metadata_summarizes_capture_return_and_exports(void);
extern void test_binary_roundtrip_preserves_escape_metadata_summaries(void);
extern void test_runtime_global_binding_marks_returned_object_as_global_root(void);
extern void test_runtime_native_callback_capture_marks_returned_object_as_native_handle(void);
extern void test_binary_roundtrip_runtime_global_binding_preserves_escape_flags(void);
extern void test_binary_roundtrip_runtime_native_callback_preserves_escape_flags(void);
extern void test_runtime_returned_callable_capture_marks_closed_capture_object(void);
extern void test_runtime_global_callable_capture_marks_closed_capture_object(void);
extern void test_binary_roundtrip_runtime_returned_callable_capture_preserves_closed_capture_escape_flags(void);
extern void test_binary_roundtrip_runtime_global_callable_capture_preserves_closed_capture_escape_flags(void);

int main(void) {
    printf("\n");
    ZR_TEST_MODULE_DIVIDER();
    printf("ZR-VM Escape Metadata Pipeline\n");
    ZR_TEST_MODULE_DIVIDER();
    printf("\n");

    UNITY_BEGIN();

    printf("==========\n");
    printf("Escape Metadata Shape Tests\n");
    printf("==========\n");
    RUN_TEST(test_compiler_escape_metadata_summarizes_capture_return_and_exports);
    RUN_TEST(test_binary_roundtrip_preserves_escape_metadata_summaries);

    printf("==========\n");
    printf("Runtime Escape Closure Tests\n");
    printf("==========\n");
    RUN_TEST(test_runtime_global_binding_marks_returned_object_as_global_root);
    RUN_TEST(test_runtime_native_callback_capture_marks_returned_object_as_native_handle);
    RUN_TEST(test_binary_roundtrip_runtime_global_binding_preserves_escape_flags);
    RUN_TEST(test_binary_roundtrip_runtime_native_callback_preserves_escape_flags);
    RUN_TEST(test_runtime_returned_callable_capture_marks_closed_capture_object);
    RUN_TEST(test_runtime_global_callable_capture_marks_closed_capture_object);
    RUN_TEST(test_binary_roundtrip_runtime_returned_callable_capture_preserves_closed_capture_escape_flags);
    RUN_TEST(test_binary_roundtrip_runtime_global_callable_capture_preserves_closed_capture_escape_flags);

    printf("\n");
    ZR_TEST_MODULE_DIVIDER();
    printf("Escape Metadata Pipeline Completed\n");
    ZR_TEST_MODULE_DIVIDER();
    printf("\n");

    return UNITY_END();
}
