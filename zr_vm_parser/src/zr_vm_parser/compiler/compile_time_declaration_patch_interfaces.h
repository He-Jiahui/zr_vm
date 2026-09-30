#ifndef ZR_VM_PARSER_COMPILE_TIME_DECLARATION_PATCH_INTERFACES_H
#define ZR_VM_PARSER_COMPILE_TIME_DECLARATION_PATCH_INTERFACES_H

#include "compiler_internal.h"

/**
 * @brief 保存待由声明 patch 事务消费的接口添加项。
 *
 * 两数组按 index 配对且长度均为 count。结构只拥有 native backing buffers；
 * typeNames 保存从已认证 TypeId 取得的 canonical 字符串借用引用，释放结构时不销毁字符串对象。
 */
typedef struct SZrParserCompileTimePatchInterfaceAdds {
    TZrTypeId *typeIds;
    SZrString **typeNames;
    TZrSize count;
} SZrParserCompileTimePatchInterfaceAdds;

/**
 * @brief 认证并暂存运行时 interfaceAdds，供声明事务继续校验与提交。
 *
 * @param cs 当前编译器状态。
 * @param targetInfo 接收新增关系的类型原型；本函数只读取既有关系。
 * @param interfaceAddsValue 运行时 interfaceAdds 数组值。
 * @param location 用于声明转换诊断的位置。
 * @param result 接收 ID/name 并行暂存数组的空结构。
 * @pre 输入对象属于当前 compiler state；result 已零初始化且不持有旧缓冲区。
 * @return 完整准备后返回 true；参数无效或任一准备步骤失败时返回 false。
 * @note 初始参数校验失败时 result 保持原状；通过参数校验后先清零 result。
 *       调用方须在每次调用后（包括失败）调用 FreePatchInterfaceAdds。
 *       本函数不发布 targetInfo 的 inherits/implements 更新。
 */
ZR_PARSER_API TZrBool ZrParser_CompileTime_PreparePatchInterfaceAdds(
        SZrCompilerState *cs,
        const SZrTypePrototypeInfo *targetInfo,
        const SZrTypeValue *interfaceAddsValue,
        SZrFileRange location,
        SZrParserCompileTimePatchInterfaceAdds *result);
/**
 * @brief 释放暂存结构拥有的数组并将结构清零。
 *
 * @param cs 分配两个 backing buffers 时对应的编译器状态。
 * @param interfaceAdds 已准备或零初始化的暂存结构。
 * @note 只释放 typeIds/typeNames 数组；typeNames 指向的 GC 字符串对象不由此函数释放。
 */
ZR_PARSER_API void ZrParser_CompileTime_FreePatchInterfaceAdds(
        SZrCompilerState *cs,
        SZrParserCompileTimePatchInterfaceAdds *interfaceAdds);

#endif // ZR_VM_PARSER_COMPILE_TIME_DECLARATION_PATCH_INTERFACES_H
