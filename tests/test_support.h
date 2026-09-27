#ifndef ZR_VM_TEST_SUPPORT_H
#define ZR_VM_TEST_SUPPORT_H

/* 保留测试层的短名称入口；实际状态与执行契约由 harness/runtime_support.h 承担。 */

#include "harness/path_support.h"
#include "harness/reference_support.h"
#include "harness/runtime_support.h"

#define ZrTests_Allocator_Default ZrTests_Runtime_Allocator_Default
#define ZrTests_State_Create ZrTests_Runtime_State_Create
#define ZrTests_State_Destroy ZrTests_Runtime_State_Destroy
#define ZrTests_Function_Execute ZrTests_Runtime_Function_Execute
#define ZrTests_Function_ExecuteExpectInt64 ZrTests_Runtime_Function_ExecuteExpectInt64

#endif
