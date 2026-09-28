#ifndef ZR_VM_TESTS_LANGUAGE_SERVER_LSP_NUMERIC_RANGE_QUERY_TEST_SUPPORT_H
#define ZR_VM_TESTS_LANGUAGE_SERVER_LSP_NUMERIC_RANGE_QUERY_TEST_SUPPORT_H

#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/state.h"

/** @brief 为数值范围查询测试的 GlobalState 提供堆分配入口。
 *  @pre 非空旧指针应来自本分配器，originalSize 应是该块的真实大小。
 *  @note 测试入口通过 GlobalState_New 注册，随后由 VM 内存层调用。 */
TZrPtr ZrVmTest_LspNumericRangeQueryAllocator(TZrPtr userData,
                                              TZrPtr pointer,
                                              TZrSize originalSize,
                                              TZrSize newSize,
                                              TZrInt64 flag);

/** @brief 在临时 LSP 文档中查询指定表达式位置，并核对精确整数范围事实。
 *  @pre state 有效；label、uriText、content、needle 是非空 NUL 结尾文本；
 *       needle 首次出现的位置加 offset 应落在目标二元表达式内，样例按 ASCII 列定位。
 *  @return 查询成功且类型、上下界、无符号界与溢出标志均符合预期时返回真。 */
TZrBool ZrVmTest_LspRunAssignmentRangeCaseAt(SZrState *state,
                                             const TZrChar *label,
                                             const TZrChar *uriText,
                                             const TZrChar *content,
                                             const TZrChar *needle,
                                             TZrSize offset,
                                             TZrInt64 expectedMin,
                                             TZrInt64 expectedMax);

#endif
