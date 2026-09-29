#ifndef ZR_VM_PARSER_COMPILER_NATIVE_CALL_BINDING_H
#define ZR_VM_PARSER_COMPILER_NATIVE_CALL_BINDING_H

#include "zr_vm_parser/compiler.h"

/** @brief 识别须由模块提供者重定位的 native 成员契约，避免把它误编成当前函数的本地常量。
 *  @note 非零模块签名哈希与 MemberDef/Signature token 是来源标记；本函数只辨认身份，不执行完整契约校验。 */
TZrBool compiler_native_call_binding_is_provider_contract(
        const SZrCallBindingContract *contract);

/** @brief 把已识别的提供者契约和成员定位值交给调用点缓存，供后续链接重定位。
 *  @pre 有效调用需提供可写 entry 和 provider 契约；memberEntryIndex 是调用点定位元数据，已知静态调用可传 ZR_PARSER_MEMBER_ID_NONE 哨兵。
 *  @note native registry 根据契约 token 与哈希选择 provider，不读取 memberEntryIndex。
 *  @return 参数或来源约束无效时返回 false，成功写入缓存时返回 true。 */
TZrBool compiler_native_call_binding_prepare_cache(
        SZrFunctionCallSiteCacheEntry *entry,
        const SZrCallBindingContract *contract,
        TZrUInt32 memberEntryIndex);

/** @brief 为 native 静态调用核对缓存中已经绑定的提供者身份及底层调用契约。
 *  @note 当前没有生产调用点；若此校验应作为编译收尾门，需先确认接入时机。
 *  @note 实际链接路径经 native registry 执行契约及目标校验；本函数未被调用，不能将其视为已执行的编译期检查。
 *  TODO: 确认是否需要在编译期接入本入口，以便更早报告 native 静态调用的无效契约。 */
TZrBool compiler_finalize_native_call_binding(
        SZrCompilerState *compiler, SZrFunctionCallSiteCacheEntry *entry);

#endif
