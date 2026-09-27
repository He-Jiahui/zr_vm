#include <stdlib.h>

#include "unity.h"

#include "reference_support.h"

/** @brief 确保全栈参考清单明确说明逐用例二进制对等执行尚未启用。
 * 样例读取器返回堆内存；读取下一个清单前须释放当前文本。
 * BUG: ReadFixture 成功后的 Unity 断言失败会跳过 free(manifestText)，
 * 该缓冲区会泄漏至此独立测试进程退出。
 */
static void test_reference_full_stack_backend_skip_reasons_use_binary_parity_wording(void) {
    static const char *kManifestPaths[] = {
        "core_semantics/lexing_literals_diagnostics/manifest.json",
        "core_semantics/expressions_precedence_chains/manifest.json",
        "core_semantics/calls_named_default_varargs/manifest.json",
        "core_semantics/types_casts_const/manifest.json",
        "core_semantics/object_member_index_construct_target/manifest.json",
        "core_semantics/protocols_iteration_comparable/manifest.json",
        "core_semantics/modules_imports_artifacts/manifest.json",
        "core_semantics/oop_inheritance_descriptors/manifest.json",
        "core_semantics/ownership_using_resource_lifecycle/manifest.json",
        "core_semantics/exceptions_gc_native_stress/manifest.json",
    };
    static const char *kDeprecatedPhrases[] = {
        "still relies on artifact/probe coverage instead of per-case manifest-driven executable parity",
        "artifact-only",
        "no native parity",
    };
    static const char *kBinarySkipReason =
        "project/runtime suites already prove binary execution; this reference case keeps per-case executable parity disabled until the manifest runner can require executed_via=binary";

    for (size_t manifestIndex = 0; manifestIndex < sizeof(kManifestPaths) / sizeof(kManifestPaths[0]); manifestIndex++) {
        TZrSize manifestSize = 0;
        TZrChar *manifestText = ZrTests_Reference_ReadFixture(kManifestPaths[manifestIndex], &manifestSize);

        (void) manifestSize;
        TEST_ASSERT_NOT_NULL(manifestText);
        for (size_t phraseIndex = 0; phraseIndex < sizeof(kDeprecatedPhrases) / sizeof(kDeprecatedPhrases[0]); phraseIndex++) {
            TEST_ASSERT_EQUAL_UINT64(
                0,
                ZrTests_Reference_CountOccurrences(manifestText, kDeprecatedPhrases[phraseIndex]));
        }
        TEST_ASSERT_TRUE(ZrTests_Reference_CountOccurrences(manifestText, kBinarySkipReason) >= 1);
        free(manifestText);
    }
}

/** @brief 作为独立 Unity 目标运行清单文案契约。 */
int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_reference_full_stack_backend_skip_reasons_use_binary_parity_wording);

    return UNITY_END();
}
