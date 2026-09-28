#ifndef ZR_VM_TESTS_LANGUAGE_SERVER_LSP_BITWISE_ZERO_MINUS_SHIFT_SUPPORTED_COUNT_RANGE_QUERY_TEST_SUPPORT_H
#define ZR_VM_TESTS_LANGUAGE_SERVER_LSP_BITWISE_ZERO_MINUS_SHIFT_SUPPORTED_COUNT_RANGE_QUERY_TEST_SUPPORT_H

#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/state.h"

/** @brief 用统一的 zero-minus/shift 源码外壳验证位或表达式的数值范围。
 *  @pre state 有效，caseName 与 expression 是非空 ASCII 文本；expression
 *       必须含目标顶层 `|`，并可放入 1024 字节源码缓冲区。
 *  @return 源码及 URI 成功格式化且共享范围查询通过时返回真。 */
TZrBool ZrVmTest_LspRunBitwiseZeroMinusShiftSupportedCountRangeQuery(
        SZrState *state,
        const TZrChar *caseName,
        const TZrChar *expression,
        TZrInt64 expectedMin,
        TZrInt64 expectedMax);

#endif
