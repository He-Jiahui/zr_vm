#ifndef ZR_VM_TESTS_PARSER_TEST_SPAN_GC_CASES_H
#define ZR_VM_TESTS_PARSER_TEST_SPAN_GC_CASES_H

/** @brief 由 span 核心 Unity 入口注册，验证请求 full GC 返回后活跃数组视图仍可写回。 */
void test_span_array_source_survives_gc_compaction_while_view_is_live(void);

#endif
