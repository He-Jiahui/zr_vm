#include <string.h>

#include "runtime_support.h"
#include "unity.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser.h"

static void assert_legacy_decorator_source_is_rejected(
        const TZrChar *source,
        const TZrChar *sourceNameText) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);
    sourceName = ZrCore_String_Create(
            state,
            (TZrNativeString)sourceNameText,
            strlen(sourceNameText));
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NULL_MESSAGE(
            function,
            "removed comptime class/@decorate syntax must not compile after the one-shot cutover");
    ZrTests_Runtime_State_Destroy(state);
}

static void test_legacy_compile_time_member_decorators_are_rejected(void) {
    static const TZrChar *source =
            "comptime class MarkField {\n"
            "    @decorate(target: typeof Field): zr.DecoratorPatch {\n"
            "        return { metadata: { compileTimeField: true } };\n"
            "    }\n"
            "}\n"
            "class User {\n"
            "    #MarkField#\n"
            "    pub var id: int = 1;\n"
            "}\n";

    assert_legacy_decorator_source_is_rejected(
            source,
            "legacy_compile_time_member_decorator.zr");
}

static void test_legacy_compile_time_parameter_decorators_are_rejected(void) {
    static const TZrChar *source =
            "comptime class MarkParameter {\n"
            "    @decorate(target: typeof Parameter): zr.DecoratorPatch {\n"
            "        return { metadata: { compileTimeParameter: true } };\n"
            "    }\n"
            "}\n"
            "pub fn load(#MarkParameter# value: int, other: int = 2): int {\n"
            "    return value + other;\n"
            "}\n";

    assert_legacy_decorator_source_is_rejected(
            source,
            "legacy_compile_time_parameter_decorator.zr");
}

static void test_legacy_decorate_meta_method_is_rejected_directly(void) {
    static const TZrChar *source =
            "class LegacyDecorator {\n"
            "    @decorate(target: object): void { }\n"
            "}\n"
            "return 0;\n";

    assert_legacy_decorator_source_is_rejected(
            source,
            "legacy_decorate_meta_method.zr");
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_legacy_compile_time_member_decorators_are_rejected);
    RUN_TEST(test_legacy_compile_time_parameter_decorators_are_rejected);
    RUN_TEST(test_legacy_decorate_meta_method_is_rejected_directly);
    return UNITY_END();
}
