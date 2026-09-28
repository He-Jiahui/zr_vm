#ifndef ZR_VM_TESTS_LANGUAGE_SERVER_TEST_LSP_BZMS_PRIVATE_NOT_MASK_OR_COUNTERPART_QUERY_CASES_H
#define ZR_VM_TESTS_LANGUAGE_SERVER_TEST_LSP_BZMS_PRIVATE_NOT_MASK_OR_COUNTERPART_QUERY_CASES_H

#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/state.h"

/** @brief 执行私有取反掩码与对应叶的组合范围查询，结果由主测试入口汇总。
 *  @pre state 是已初始化的测试主线程状态；调用方负责其生存期。
 *  @return 全部局部表达式范围用例均通过时返回真。 */
TZrBool ZrVmTest_LspRunBzmsPrivateNotMaskOrCounterpartQueries(SZrState *state);

#endif
