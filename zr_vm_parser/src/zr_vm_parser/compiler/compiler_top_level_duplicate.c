#include "compiler_top_level_duplicate.h"

#include "compiler_internal.h"

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

        if (prototype == ZR_NULL || prototype->name == ZR_NULL ||
            prototype->declarationNode == declaration ||
            !ZrCore_String_Equal(prototype->name, name)) {
            continue;
        }

        (void)ZrParser_Compiler_ReportDuplicateTypeDeclaration(
                cs,
                declaration,
                prototype->declarationNode);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}
