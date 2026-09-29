#ifndef ZR_VM_PARSER_COMPILE_TIME_DECLARATION_PATCH_ATTRIBUTES_H
#define ZR_VM_PARSER_COMPILE_TIME_DECLARATION_PATCH_ATTRIBUTES_H

#include "compiler_internal.h"

/** @brief 单个 Patch AttributeData 的规范化暂存项。
 * @note data.fieldValues 与 values 由外层 adds aggregate 持有；schema 借用 compiler state。
 */
typedef struct SZrParserCompileTimePatchAttributeAdd {
    SZrParserAttributeData data; /* 交给 declaration-patch 校验的值；fieldValues 由本项缓冲区拥有。 */
    const SZrCompilerAttributeSchemaBinding *schema; /* 借用 compiler state 的已注册 schema。 */
    SZrTypeValue *values; /* 与 schema.fields 同序的 metadata 写入值，由 aggregate 释放。 */
} SZrParserCompileTimePatchAttributeAdd;

/** @brief 一批属性添加的暂存所有者，供准备、事务构建和统一释放阶段传递。
 * @note contractData 中的 fieldValues 指向 entries 内的缓冲区，不可独立释放或越过 Free 使用。
 */
typedef struct SZrParserCompileTimePatchAttributeAdds {
    SZrParserCompileTimePatchAttributeAdd *entries; /* 单项及其字段缓冲的所有者。 */
    SZrParserAttributeData *contractData; /* entries.data 的浅拷贝，仅供当前事务借用。 */
    TZrSize count; /* entries 与 contractData 的共同元素数。 */
} SZrParserCompileTimePatchAttributeAdds;

/**
 * @brief 校验并规范化 declaration.Patch.attributeAdds，生成可交给事务层验证的暂存项。
 * @pre 所有参数非空；result 应先归零初始化，且与 attributeAddsValue 属于同一 compiler state。
 * @return 成功时 entries 和 contractData 由 result 持有；调用方须在所有后续路径调用 Free。
 * @note 无效参数早退不改 result；有效参数先清空 result，预算超限可能留下非零 count；暂存分配 OOM 返回 false 且不设置 compiler 错误标志。
 */
ZR_PARSER_API TZrBool ZrParser_CompileTime_PreparePatchAttributeAdds(
        SZrCompilerState *cs,
        const SZrTypePrototypeInfo *targetInfo,
        const SZrTypeValue *attributeAddsValue,
        SZrFileRange location,
        SZrParserCompileTimePatchAttributeAdds *result);
/**
 * @brief 从既有 decorator metadata 与已验证添加项构造独立的 overlay 对象。
 * @pre 输入 aggregate 仍有效；metadataValue 是输出槽，成功后调用方应在下一次 GC 分配前接入根。
 * @return 成功时输出对象值；空添加批次成功输出 null。参数有效后失败会保持 null，参数无效早退不改输出。
 */
TZrBool ZrParser_CompileTime_BuildPatchAttributeMetadata(
        SZrCompilerState *cs,
        const SZrTypePrototypeInfo *targetInfo,
        const SZrParserCompileTimePatchAttributeAdds *attributeAdds,
        SZrTypeValue *metadataValue);
/** @brief 释放 PreparePatchAttributeAdds 生成的全部原生暂存并将 aggregate 归零。
 * @pre cs 必须是创建该 aggregate 的 compiler state，且其 GC/global 状态仍存活。
 */
void ZrParser_CompileTime_FreePatchAttributeAdds(
        SZrCompilerState *cs,
        SZrParserCompileTimePatchAttributeAdds *attributeAdds);

#endif // ZR_VM_PARSER_COMPILE_TIME_DECLARATION_PATCH_ATTRIBUTES_H
