#ifndef ZR_VM_PARSER_COMPILER_CALL_BINDING_H
#define ZR_VM_PARSER_COMPILER_CALL_BINDING_H

#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/type_inference.h"

/** @brief 将类型成员的解析信息投影为调用点 dispatch fact。
 *  @pre fact 指向可写输出；member 可为空；provider 合约会被原样保留。
 */
void compiler_get_member_call_binding_fact(SZrCompilerState *compiler,
        const SZrTypeMemberInfo *member, SZrCallBindingContract *fact);

/** @brief 返回 core call-binding 使用的 typed callable 结构签名哈希；失败时返回 0。
 *  @note 此接口只计算身份，不记录诊断或修改 call-site cache。
 */
TZrUInt64 compiler_typed_call_signature_hash(
        SZrCompilerState *compiler,
        const SZrResolvedCallSignature *signature);

/** @brief 把一个解析完成的 typed callable 调用登记为待 finalize 的 cache contract。
 *  @return 参数/签名不匹配、签名哈希无效或 cache 扩展失败时返回 false。
 */
TZrBool compiler_record_typed_call_binding(
        SZrCompilerState *compiler,
        const SZrResolvedCallSignature *signature,
        TZrUInt32 argumentCount,
        SZrFileRange location);

/** @brief 定稿函数图中的调用绑定，并依次发布跨模块身份、执行 core linker。
 *  @note 由总编译入口在 quickening 和 source-module 定稿之后调用；失败时调用方释放该函数图。
 */
TZrBool compiler_finalize_call_bindings(SZrCompilerState *compiler, SZrFunction *function);
/** @brief 为导入方发布 provider callable 常量、child 函数和 prototype owner 的可重定位身份。
 *  @note 由 compiler_finalize_call_bindings 在本地契约定稿之后、当前函数 core 链接之前调用；
 *        不独立决定运行时目标，导入/链接消费者仍按 metadata token、签名和布局契约验证。
 */
TZrBool compiler_publish_module_call_bindings(SZrCompilerState *compiler, SZrFunction *function);

#endif
