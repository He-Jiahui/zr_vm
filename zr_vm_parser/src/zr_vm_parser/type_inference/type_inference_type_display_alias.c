#include "type_inference_type_display_alias.h"

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/semantic_display.h"
#include "type_inference_internal.h"

void type_inference_publish_primitive_type_display_alias(
        SZrCompilerState *cs,
        const SZrInferredType *type,
        const SZrType *typeUse) {
    const SZrAstNode *typeUseNode;
    SZrString *alias;
    TZrNativeString aliasText;
    TZrSize aliasLength;
    EZrValueType primitiveType;
    TZrTypeId typeId;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || type == ZR_NULL ||
        typeUse == ZR_NULL || typeUse->name == ZR_NULL ||
        typeUse->name->type != ZR_AST_IDENTIFIER_LITERAL) {
        return;
    }
    typeUseNode = typeUse->name;
    alias = typeUseNode->data.identifier.name;
    if (alias == ZR_NULL) {
        return;
    }
    aliasText = ZrCore_String_GetNativeString(alias);
    aliasLength = alias->shortStringLength < ZR_VM_LONG_STRING_FLAG
                          ? alias->shortStringLength
                          : alias->longStringLength;
    if (aliasText == ZR_NULL ||
        !inferred_type_try_map_primitive_name(
                aliasText, aliasLength, &primitiveType)) {
        return;
    }
    typeId = ZrParser_CanonicalType_FromInferred(cs->semanticContext, type);
    if (typeId == ZR_SEMANTIC_ID_INVALID) {
        return;
    }
    (void)ZrParser_SemanticTypeDisplayAlias_Publish(
            cs->semanticContext, typeId, &typeUseNode->location, alias);
}
