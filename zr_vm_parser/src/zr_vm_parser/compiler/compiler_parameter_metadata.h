#ifndef ZR_VM_PARSER_COMPILER_PARAMETER_METADATA_H
#define ZR_VM_PARSER_COMPILER_PARAMETER_METADATA_H

#include "compiler_internal.h"

/** @brief 把编译期参数元数据写入已受 GC 根保护的描述符，供接口成员和 union payload 共享。
 *  @pre cs/state、descriptor、parameter 有效；调用方维持 descriptor 的临时根。
 *  @return 可检测的分配、临时根或字段 helper 失败时返回 false；其余路径返回 true。 */
TZrBool compiler_parameter_metadata_write_descriptor(
        SZrCompilerState *cs,
        SZrObject *descriptor,
        const SZrFunctionMetadataParameter *parameter,
        TZrUInt32 position);

/** @brief 将接口方法或 meta 签名的参数快照挂到成员 decorator metadata 的指定字段。
 *  @pre cs/state、memberInfo、fieldName 有效；functionNode 与 params 属同一签名。
 *  @return 可检测的参数构建、GC 根或字段 helper 失败时返回 false，成功后 memberInfo 持有 metadata 对象。 */
TZrBool compiler_parameter_metadata_attach_member_array(
        SZrCompilerState *cs,
        SZrTypeMemberInfo *memberInfo,
        SZrAstNodeArray *params,
        SZrAstNode *functionNode,
        const TZrChar *fieldName);

#endif
