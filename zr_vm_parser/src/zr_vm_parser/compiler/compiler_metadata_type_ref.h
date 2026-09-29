#ifndef ZR_VM_PARSER_COMPILER_METADATA_TYPE_REF_H
#define ZR_VM_PARSER_COMPILER_METADATA_TYPE_REF_H

#include "compiler_internal.h"

/**
 * @brief TYPE_REF 规划阶段输出的表项数与签名堆字节数。
 * @note typeRefCount 是 TypeRef 行数；每行发射时还配一条 Signature 行。
 */
typedef struct SZrMetadataExternalTypeRefPlan {
    TZrUInt32 typeRefCount; /**< 待发射的外部 TypeRef 数，不含配对 Signature 行。 */
    TZrSize signatureHeapLength; /**< 所有 TypeRef 原始签名字节长度之和。 */
} SZrMetadataExternalTypeRefPlan;

/**
 * @brief 解析一个导入目标时返回的签名与 provider 身份视图。
 * @note metadataToken/signatureToken/hash 标识 provider 导出项；parameterTypes 是借用数组，
 *       解析器及其缓存须在消费者完成本轮 plan/emit 前保持有效。
 */
typedef struct SZrMetadataTokenTargetSignature {
    TZrUInt8 symbolKind; /**< provider 导出符号类别。 */
    TZrUInt8 exportKind; /**< provider 导出种类。 */
    TZrUInt8 readiness; /**< provider 摘要中的就绪状态。 */
    TZrUInt8 reserved0; /**< 保留字段。 */
    SZrFunctionTypedTypeRef valueType; /**< 目标值/返回类型；TypeRef 收集使用其类型树。 */
    TZrUInt32 genericParameterCount; /**< 目标 callable 的泛型形参数量。 */
    TZrUInt32 parameterCount; /**< parameterTypes 中有效参数类型的数量。 */
    SZrFunctionTypedTypeRef *parameterTypes; /**< 借用的参数类型数组，不转移所有权。 */
    TZrMetadataToken metadataToken; /**< provider 导出项的 metadata token。 */
    TZrMetadataToken signatureToken; /**< provider 导出项关联的签名 token。 */
    TZrUInt64 signatureHash; /**< provider 导出签名 hash。 */
    TZrBool hasSignature; /**< true 时 valueType 与参数视图可供收集使用。 */
} SZrMetadataTokenTargetSignature;
/**
 * @brief 按导入 effect 查找 provider 导出签名。
 * @return ZR_FALSE 表示无法解析；ZR_TRUE 且 outSignature->hasSignature 为真时才参与 TypeRef 收集。
 * @note 输出中的参数数组由 resolver 借用，必须覆盖当前规划/发射调用。
 */
typedef TZrBool (*TZrMetadataTypeRefTargetSignatureResolver)(SZrCompilerState *cs,
                                                              const SZrFunctionModuleEffect *effect,
                                                              SZrMetadataTokenTargetSignature *outSignature);

/**
 * @brief 将扁平 effect 索引映射到对应导入 effect。
 * @return 返回借用的 effect 指针；索引无效或条目缺失时返回 ZR_NULL。
 */
typedef const SZrFunctionModuleEffect *(*TZrMetadataTypeRefEffectByFlatIndex)(const SZrFunction *function,
                                                                              TZrUInt32 effectIndex);

/**
 * @brief 将 TYPE_REF 的模块名解析为当前输出中的 AssemblyRef RID。
 * @return 返回一基 AssemblyRef RID；0 表示当前模块集合中没有可用条目。
 * @note userData 只在 emit 回调期间借用；resolver 可结合 effectAt 处理显式导入模块。
 */
typedef TZrUInt32 (*TZrMetadataTypeRefAssemblyRefRidResolver)(SZrCompilerState *cs,
                                                              const SZrFunction *function,
                                                              TZrUInt32 totalEffectCount,
                                                              TZrMetadataTypeRefEffectByFlatIndex effectAt,
                                                              SZrString *moduleName,
                                                              void *userData);

/**
 * @brief 按泛型层级之外最后一个点拆分模块限定类型名。
 * @pre cs/state、typeName 及两个输出指针地址有效。
 * @return 成功创建非空 module/member 字符串时为 true；失败时调用者必须忽略输出值。
 * @note 输出在检查前清零；第二个字符串分配失败时第一个 state-owned 字符串可能已创建。
 */
TZrBool compiler_metadata_type_ref_split_module_qualified_type(SZrCompilerState *cs,
                                                               SZrString *typeName,
                                                               SZrString **outModuleName,
                                                               SZrString **outMemberTypeName);

/**
 * @brief 将无模块限定的 value alias 解析成模块名与目标成员名。
 * @pre cs/state、typeName 及两个输出指针地址有效；带顶层模块限定的名称不由此函数处理。
 * @return 找到可拆分的模块限定 alias 目标时为 true；返回字符串归 state 管理。
 * @note 若输入是泛型实例，原实参列表会附到 alias 目标基名后；临时数组在返回前释放。
 */
TZrBool compiler_metadata_type_ref_resolve_unqualified_alias(SZrCompilerState *cs,
                                                             SZrString *typeName,
                                                             SZrString **outModuleName,
                                                             SZrString **outMemberTypeName);

/**
 * @brief 统计将要发射的外部 TYPE_REF 行数及其签名堆长度。
 * @pre cs/state/global 与 outPlan 有效；effect、目标 resolver 及其数据在随后的 emit 前保持稳定。
 * @param totalEffectCount effectAt 可访问的扁平导入 effect 数量。
 * @param effectAt 提供与 emit 相同顺序的导入 effect 视图。
 * @param resolveTarget 解析导入 effect 对应的目标签名视图。
 * @param stringHeapEntries 与 stringHeapEntryCount 为 emit API 配对保留，当前规划 pass 不读取。
 * @return 无效上下文、分配/收集失败或签名长度超出 u32 范围时为 false；失败输出不得使用。
 * @note 成功后 outPlan 是值类型快照，不拥有动态内存；发射阶段会重新收集同一组条目。
 */
TZrBool compiler_metadata_type_ref_plan(SZrCompilerState *cs,
                                        const SZrFunction *function,
                                        TZrUInt32 totalEffectCount,
                                        TZrMetadataTypeRefEffectByFlatIndex effectAt,
                                        TZrMetadataTypeRefTargetSignatureResolver resolveTarget,
                                        const SZrMetadataStringHeapEntry *stringHeapEntries,
                                        TZrUInt32 stringHeapEntryCount,
                                        SZrMetadataExternalTypeRefPlan *outPlan);

/**
 * @brief 为已规划的外部类型追加签名字节及成对的 TYPE_REF/SIGNATURE 记录。
 * @pre 输出记录表、堆和游标有足够容量；输入函数/effect/resolver 与成功 plan 时保持一致。
 * @param resolveAssemblyRefRid 可选的模块到 AssemblyRef RID resolver；为空时按 effect 顺序查找。
 * @param resolveAssemblyRefRidUserData 仅在本次 resolver 调用期间借用的上下文。
 * @param ioRecordIndex 指向下一对可用记录的索引，成功写一项会推进两条记录。
 * @param ioHeapOffset 指向签名堆尾部，成功写一项会推进该签名长度。
 * @param ioSignatureRidCursor Signature 表的一基 RID 游标。
 * @param ioTypeRefRidCursor TypeRef 表的一基 RID 游标。
 * @param stringHeapEntries 发射原始 TYPE_REF 签名所需的稳定字符串索引表。
 * @return 上下文无效、临时分配/收集失败、AssemblyRef 无法解析、输出范围不足或签名校验失败时为 false。
 * @note false 时不回滚已推进的游标或已写记录/堆字节；调用者须丢弃整个临时 metadata 输出。
 */
TZrBool compiler_metadata_type_ref_emit(SZrCompilerState *cs,
                                        const SZrFunction *function,
                                        TZrUInt32 totalEffectCount,
                                        TZrMetadataTypeRefEffectByFlatIndex effectAt,
                                        TZrMetadataTypeRefTargetSignatureResolver resolveTarget,
                                        TZrMetadataTypeRefAssemblyRefRidResolver resolveAssemblyRefRid,
                                        void *resolveAssemblyRefRidUserData,
                                        SZrMetadataTokenRecord *records,
                                        TZrUInt32 recordCount,
                                        TZrUInt32 *ioRecordIndex,
                                        TZrByte *heap,
                                        TZrSize heapLength,
                                        TZrSize *ioHeapOffset,
                                        TZrUInt32 *ioSignatureRidCursor,
                                        TZrUInt32 *ioTypeRefRidCursor,
                                        const SZrMetadataStringHeapEntry *stringHeapEntries,
                                        TZrUInt32 stringHeapEntryCount);

#endif
