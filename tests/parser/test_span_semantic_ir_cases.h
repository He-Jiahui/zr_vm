#ifndef ZR_VM_TEST_SPAN_SEMANTIC_IR_CASES_H
#define ZR_VM_TEST_SPAN_SEMANTIC_IR_CASES_H

/** @brief 验证 owner MOVE 与 native-pinned DROP 不能越过活跃连续视图借用。 */
void test_span_owner_move_and_native_drop_conflict_with_active_view(void);

#endif /* ZR_VM_TEST_SPAN_SEMANTIC_IR_CASES_H */
