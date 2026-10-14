//
// zr.system.env descriptor registry.
//

#ifndef ZR_VM_LIB_SYSTEM_ENV_REGISTRY_H
#define ZR_VM_LIB_SYSTEM_ENV_REGISTRY_H

#include "zr_vm_lib_system/env.h"

/**
 * ## Summary
 * 提供 zr.system.env 的静态原生模块描述符，供系统模块注册阶段发现 getVariable callback 与类型提示。
 *
 * ## Parameters
 * 无参数；该 getter 只返回模块内固定的描述符，不读取调用时状态。
 *
 * ## Returns
 * @return 指向进程映像内静态只读 ZrLibModuleDescriptor 的借用指针；不转移所有权，调用者不得释放或修改。
 *
 * ## Constraints and lifecycle
 * 返回的描述符及其函数表、提示表和 JSON 提示在 provider image 存活期间有效。注册器可能保留嵌套指针或 callback；
 * provider image 必须至少存活到这些借用结束，浅拷贝 descriptor 不会延长 image 生命周期。查询不分配内存，也不创建新副本。
 *
 * ## References
 * ZrVmLibSystem_Register 通过此 getter 注册环境模块；ZrSystem_Env_GetVariable 是描述符所绑定的查询 callback。
 */
const ZrLibModuleDescriptor *ZrSystem_EnvRegistry_GetModule(void);

#endif // ZR_VM_LIB_SYSTEM_ENV_REGISTRY_H
