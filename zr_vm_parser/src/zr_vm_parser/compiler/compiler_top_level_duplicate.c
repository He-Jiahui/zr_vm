#include "compiler_top_level_duplicate.h"

#include "compiler_internal.h"

/**
 * @brief 从受支持的顶层类型声明提取统一名称，供跨声明种类的重名检查使用。
 * @note 非类型语句或缺失名称返回 NULL；类别分派与诊断侧的声明身份提取保持同一组 AST 类型。
 */
static SZrString *compiler_top_level_type_name(SZrAstNode *declaration) {
    if (declaration == ZR_NULL) {
        return ZR_NULL;
    }
    switch (declaration->type) {
        case ZR_AST_CLASS_DECLARATION:
            return declaration->data.classDeclaration.name != ZR_NULL
                           ? declaration->data.classDeclaration.name->name
                           : ZR_NULL;
        case ZR_AST_STRUCT_DECLARATION:
            return declaration->data.structDeclaration.name != ZR_NULL
                           ? declaration->data.structDeclaration.name->name
                           : ZR_NULL;
        case ZR_AST_INTERFACE_DECLARATION:
            return declaration->data.interfaceDeclaration.name != ZR_NULL
                           ? declaration->data.interfaceDeclaration.name->name
                           : ZR_NULL;
        case ZR_AST_ENUM_DECLARATION:
            return declaration->data.enumDeclaration.name != ZR_NULL
                           ? declaration->data.enumDeclaration.name->name
                           : ZR_NULL;
        case ZR_AST_UNION_DECLARATION:
            return declaration->data.unionDeclaration.name != ZR_NULL
                           ? declaration->data.unionDeclaration.name->name
                           : ZR_NULL;
        default:
            return ZR_NULL;
    }
}

/**
 * @brief 在已登记的 type prototype 中查找另一个同名声明并报告重复绑定。
 * @note 该检查同时运行于 interface signature 预登记和顶层扩展编译；已登记的同一 AST 节点是跨阶段重遇，不是重复声明。
 * @return 找到不同 AST 节点的同名 prototype 时返回 true，让编译器跳过这次声明；否则返回 false。
 */
TZrBool compiler_report_duplicate_top_level_type(
        SZrCompilerState *cs,
        SZrAstNode *declaration) {
    SZrString *name = compiler_top_level_type_name(declaration);

    if (cs == ZR_NULL || cs->state == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0U; index < cs->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *prototype =
                (SZrTypePrototypeInfo *)ZrCore_Array_Get(
                        &cs->typePrototypes,
                        index);

        /* interface signature 阶段已登记的同一 AST 会在后续编译阶段再次遇到，不应当作另一声明。 */
        if (prototype == ZR_NULL || prototype->name == ZR_NULL ||
            prototype->declarationNode == declaration ||
            !ZrCore_String_Equal(prototype->name, name)) {
            continue;
        }

        /* 结构化诊断构造失败时 reporter 会回退普通 compiler error；仍返回 true 阻止重复声明进入后续注册。 */
        (void)ZrParser_Compiler_ReportDuplicateTypeDeclaration(
                cs,
                declaration,
                prototype->declarationNode);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}
