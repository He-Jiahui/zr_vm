#ifndef ZR_VM_PARSER_COMPILER_CALL_BINDING_H
#define ZR_VM_PARSER_COMPILER_CALL_BINDING_H

#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/type_inference.h"

void compiler_get_member_call_binding_fact(SZrCompilerState *compiler,
        const SZrTypeMemberInfo *member, SZrCallBindingContract *fact);

TZrUInt64 compiler_typed_call_signature_hash(
        SZrCompilerState *compiler,
        const SZrResolvedCallSignature *signature);

TZrBool compiler_record_typed_call_binding(
        SZrCompilerState *compiler,
        const SZrResolvedCallSignature *signature,
        TZrUInt32 argumentCount,
        SZrFileRange location);

TZrBool compiler_finalize_call_bindings(SZrCompilerState *compiler, SZrFunction *function);
/** @brief 为导入方发布 provider callable 常量、child 函数和 prototype owner 的可重定位身份。
 *  @note 由 compiler_finalize_call_bindings 在本地契约定稿之后、当前函数 core 链接之前调用；
 *        不独立决定运行时目标，导入/链接消费者仍按 metadata token、签名和布局契约验证。
 */
TZrBool compiler_publish_module_call_bindings(SZrCompilerState *compiler, SZrFunction *function);

#endif
