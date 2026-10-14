//
// zr.system.env native callbacks.
//

#ifndef ZR_VM_LIB_SYSTEM_ENV_H
#define ZR_VM_LIB_SYSTEM_ENV_H

#include "zr_vm_lib_system/conf.h"

/**
 * ## Summary
 * 在 native callback 阶段查询宿主进程当前的环境变量；变量缺失时交付 VM null，存在时把值复制为 VM 字符串。
 * 此接口只读取宿主环境，不维护模块自己的快照或环境变量存储。
 *
 * ## Parameters
 * @param[in] context 原生调用上下文；第 0 个参数必须是变量名 VM 字符串。字符串对象在 callback 期间借用，
 *                    不转移所有权。
 * @param[out] result 原生调用结果槽；派发先将槽初始化为 VM null，callback 随后写入缺失标记或 VM 管理的字符串，
 *                    不由调用者释放。
 *
 * ## Returns
 * @return ZR_TRUE 表示 callback 未报告失败；变量不存在时正常写入 null。ZR_FALSE 表示 context、result 或防御性名称检查失败。
 *         注册调用的参数个数先由 dispatcher 按 descriptor 校验；个数不符会在进入 callback 前抛出元数异常。恰有一个参数但类型错误时，
 *         ReadString 抛出类型异常；这些异常都不是普通 ZR_FALSE 返回。
 * BUG: 已存在的短变量值在字符串驻留表扩容分配失败时也可能保留预置 null 并返回 ZR_TRUE，见实现中的失败链说明。
 *
 * ## Constraints and lifecycle
 * 环境查询发生在调用时；getenv 返回的宿主指针只在本次同步复制期间借用，不得保存。变量名按 NUL 结尾的
 * C 字符串解释；若 VM 字符串含嵌入 NUL，getenv 只查询第一个 NUL 前的名称前缀；需要完整名称时应传入不含 NUL
 * 的变量名。callback 不锁定宿主环境；调用方须遵循目标 CRT 对并发环境修改的同步约定，并保证查询到复制完成期间
 * 返回值有效。SetString 的对象分配可能在底层分配失败后触发完整 GC；GC 函数槽重写中的调试跟踪还可能再次调用
 * getenv。该嵌套调用后前次 getenv 指针是否仍有效尚待按受支持 CRT 核实，见实现 TODO；成功创建的结果由 VM 管理。
 *
 * ## References
 * ZrSystem_EnvRegistry_GetModule 将此 callback 以一个必需参数注册为 zr.system.env.getVariable；native dispatcher
 * 在调用前检查元数，ZrLib_CallContext_ReadString 检查字符串类型；ZrLib_Value_SetString 复制结果文本并交给 VM。
 */
TZrBool ZrSystem_Env_GetVariable(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_ENV_H
