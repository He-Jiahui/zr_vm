//
// zr.system.exception native callbacks.
//

#ifndef ZR_VM_LIB_SYSTEM_EXCEPTION_H
#define ZR_VM_LIB_SYSTEM_EXCEPTION_H

#include "zr_vm_lib_system/conf.h"

/**
 * ## Summary
 * 在当前 VM 的全局状态中登记一个未处理异常观察函数，供负责异常出口的宿主稍后读取和调用。
 * 该回调由 native 函数描述符公开；本函数只保存回调，不负责执行它。
 *
 * ## Parameters
 * @param[in] context 当前 VM 调用的有效 native context；参数 0 接受 FUNCTION、CLOSURE 或 NATIVE_POINTER 值。
 * @param[out] result 必须非空且可写的 native 返回槽；成功时写 VM null，返回 false 前不修改该槽。
 *
 * ## Returns
 * @return 成功复制回调到全局槽并设置 null 结果时返回 ZR_TRUE；context、result、state 或 global 无效时返回 ZR_FALSE。
 *         有效 context 的元数/类型错误沿 VM 异常路径传播，不作为参数读取的普通 false 返回。
 *         此布尔值表示注册 callback 成功，不表示 callback 已运行或已消费异常。
 *
 * ## Constraints and lifecycle
 * native 描述符要求恰好一个参数，常规 VM dispatch 会在进入 callback 前检查元数；本函数再读取该参数并要求
 * 它是 ReadFunction 接受的 VM 函数、闭包或原生指针。登记槽是每个 global 的单一值，后续注册替换旧值；GC 追踪槽内 VM 值。
 * session 仅在函数/闭包及其可达图满足可恢复条件时保存/恢复该槽；NATIVE_POINTER 或 native closure 会使 capture 失败。
 * 注册不校验 hint 的 `(Error) -> bool` 签名或原生地址；实际原生回调须符合 `FZrNativeFunction` ABI，裸指针还须非空；原生指针或 native closure 所指向的代码及资源在 global 槽
 * 仍可能引用它时有效；GC 不延长原生代码映像的寿命。仓内当前只有测试
 * harness 实际分发该槽；生产 CLI/REPL 出口未读取它，调用方不能把注册成功视作宿主已完成分发。
 * 该槽是 global 共享状态；本入口未提供跨 mutator 原子替换或并发派发保证。
 * TODO: 跨线程使用前，沿 State_MutatorLaunch、native dispatch 与实际宿主消费者核实当前 VM 协调协议。
 *
 * ## References
 * `zr_vm_lib_system/src/zr_vm_lib_system/exception/exception_registry.c:kFunctions` 提供 native 绑定；
 * `zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1255` 定义 ReadFunction 接受集合；`zr_vm_core/src/zr_vm_core/session/session_checkpoint_capture.c:35` 限定恢复条件；
 * `zr_vm_core/include/zr_vm_core/value.h:15` 与 `zr_vm_core/src/zr_vm_core/function/function.c:4273` 定义和调用原生函数 ABI；
 * `tests/harness/runtime_support.c:213` 实际读取并调用槽；
 * `zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:367` 与 `zr_vm_cli/src/zr_vm_cli/repl/repl_session.c:38` 是已核对的生产失败出口。
 */
TZrBool ZrSystem_Exception_RegisterUnhandledException(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 初始化 `Error` 及其六个派生异常类共享的 `stacks`、`message` 与 `exception` 实例字段；由类型描述符的构造元方法调用。
 *
 * ## Parameters
 * @param[in] context 当前 VM 调用的有效 native context；`self` 必须是新建的异常对象，位置参数为零个或一个消息值。
 * @param[out] result 必须非空且可写的 native 构造结果槽；成功写 VM null，返回 false 前不修改该槽；实例由反射构造路径交付。
 *
 * ## Returns
 * @return 完成构造字段处理时返回 ZR_TRUE；context、result、state、接收者校验或必需数组创建失败时返回 ZR_FALSE。
 *         直接调用也检查 0..1 元数，错误可沿 VM 异常路径传播。布尔返回值不是构造实例。
 *
 * ## Constraints and lifecycle
 * `kExceptionConstructors` 为 Error 与所有派生异常类注册 0..1 元数。省略消息会写入 VM null；显式传入的值经
 * `ZrCore_Value_ConvertToString` 转换，可能运行脚本转换协议并沿 native dispatch 传播 VM 异常。`stacks` 在这里
 * 初始化为空数组，异常规范化/抛出路径另行维护诊断内容。字段写入使用 void setter，不能据 callback 成功推断每次
 * 写入都已落入对象；`stacks` 写入有一条受限的 ignored-root 扩容软失败路径，见实现处 BUG 注释。
 * 消息转换若沿 VM 异常退出，receiver 可能已有部分字段写入；本 callback 不提供字段回滚。
 *
 * ## References
 * `zr_vm_lib_system/src/zr_vm_lib_system/exception/exception_registry.c:kExceptionConstructors` 把此 callback 绑定到异常类；
 * `zr_vm_core/src/zr_vm_core/reflection/field/reflection_construction.c:822` 同步调用构造器并在成功后交付接收者；
 * `tests/exceptions/test_exceptions.c:221` 覆盖 RuntimeError 构造后的类型捕获，但不断言这些字段。
 */
TZrBool ZrSystem_Exception_Constructor(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_EXCEPTION_H
