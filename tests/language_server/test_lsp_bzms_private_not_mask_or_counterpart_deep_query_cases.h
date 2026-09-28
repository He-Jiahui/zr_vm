#ifndef ZR_VM_TESTS_LANGUAGE_SERVER_TEST_LSP_BZMS_PRIVATE_NOT_MASK_OR_COUNTERPART_DEEP_QUERY_CASES_H
#define ZR_VM_TESTS_LANGUAGE_SERVER_TEST_LSP_BZMS_PRIVATE_NOT_MASK_OR_COUNTERPART_DEEP_QUERY_CASES_H

#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/state.h"

/** @brief 将更深的对应叶表达式矩阵加入私有取反掩码主测试。
 *  @pre state 是已初始化的测试主线程状态；调用方负责其生存期。
 *  @return 全部深层组合的局部范围查询均通过时返回真。 */
TZrBool ZrVmTest_LspRunBzmsPrivateNotMaskOrCounterpartDeepQueries(SZrState *state);

#endif
