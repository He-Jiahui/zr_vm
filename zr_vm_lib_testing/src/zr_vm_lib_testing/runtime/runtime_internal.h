#ifndef ZR_VM_LIB_TESTING_RUNTIME_INTERNAL_H
#define ZR_VM_LIB_TESTING_RUNTIME_INTERNAL_H

#include "zr_vm_lib_testing/module.h"

/**
 * ## Summary
 * 约定module入口到runtime描述表的私有查询边界；公开头提供共用descriptor类型，私有头不进入安装API。
 *
 * ## Returns
 * @return 非NULL的静态const借用表；嵌套数组/字符串也由provider持有，调用者不释放。
 *
 * ## Constraints and lifecycle
 * 无需VM或registry；不注册、不物化、不切phase。provider须覆盖注册/closure消费期，不能在借用尚存时卸载。
 *
 * ## References
 * zr_vm_lib_testing/src/zr_vm_lib_testing/module.c:23 直接调用，zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c:594 交付表；zr_vm_lib_testing/CMakeLists.txt:25 仅安装公开头。
 */
const ZrLibModuleDescriptor *ZrVmLibTesting_Runtime_GetModuleDescriptor(void);

#endif
