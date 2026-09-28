#include "metadata/lsp_external_metadata_identity.h"

#include <string.h>

/* parser 的目标类别与导入原型的具体表示并非一一对应；本门禁只判定可参与身份匹配的类别。 */
static TZrBool external_metadata_identity_kind_matches(
        SZrSemanticAnalyzer *analyzer,
        EZrSemanticExternalTargetKind targetKind,
        const SZrTypeMemberInfo *candidate) {
    const SZrTypePrototypeInfo *projectedType;

    if (candidate == ZR_NULL) {
        return ZR_FALSE;
    }
    projectedType = analyzer != ZR_NULL && candidate->fieldTypeName != ZR_NULL
            ? ZrLanguageServer_LspModuleMetadata_FindTypePrototype(
                    analyzer,
                    ZrCore_String_GetNativeString(candidate->fieldTypeName))
            : ZR_NULL;
    switch (targetKind) {
        case ZR_SEMANTIC_EXTERNAL_TARGET_MODULE:
            return projectedType != ZR_NULL &&
                   projectedType->type == ZR_OBJECT_PROTOTYPE_TYPE_MODULE;
        case ZR_SEMANTIC_EXTERNAL_TARGET_CALLABLE:
            return candidate->moduleExportKind == ZR_MODULE_EXPORT_KIND_FUNCTION ||
                   candidate->memberType == ZR_AST_CLASS_METHOD ||
                   candidate->memberType == ZR_AST_STRUCT_METHOD ||
                   candidate->memberType ==
                           ZR_AST_INTERFACE_METHOD_SIGNATURE;
        case ZR_SEMANTIC_EXTERNAL_TARGET_TYPE:
            return candidate->moduleExportKind == ZR_MODULE_EXPORT_KIND_TYPE ||
                   (projectedType != ZR_NULL &&
                    projectedType->type != ZR_OBJECT_PROTOTYPE_TYPE_MODULE);
        case ZR_SEMANTIC_EXTERNAL_TARGET_VALUE:
        case ZR_SEMANTIC_EXTERNAL_TARGET_FIELD:
            return candidate->moduleExportKind == ZR_MODULE_EXPORT_KIND_VALUE;
        case ZR_SEMANTIC_EXTERNAL_TARGET_PROPERTY:
            return candidate->memberType == ZR_AST_CLASS_PROPERTY ||
                   candidate->memberType == ZR_AST_INTERFACE_PROPERTY_SIGNATURE ||
                   candidate->memberType == ZR_AST_PROPERTY_DECLARATION;
        default:
            return ZR_FALSE;
    }
}

/* 同名成员可能具有不同签名；跨快照消费者只能信任完整的所有者与元数据身份。 */
static TZrBool external_metadata_identity_matches(
        SZrSemanticAnalyzer *analyzer,
        const SZrParserSemanticExternalReferenceQuery *identity,
        const SZrTypeMemberInfo *candidate) {
    return identity != ZR_NULL && candidate != ZR_NULL &&
           external_metadata_identity_kind_matches(
                   analyzer, identity->externalTargetKind, candidate) &&
           candidate->ownerTypeName != ZR_NULL &&
           ZrCore_String_Equal(
                   identity->externalOwnerIdentity, candidate->ownerTypeName) &&
           identity->externalMetadataToken == candidate->metadataToken &&
           identity->externalSignatureToken == candidate->signatureToken &&
           identity->externalSignatureHash == candidate->signatureHash;
}

/* 供语义查询在 AST 不可用时恢复外部成员；先按 parser 保存的身份唯一化，再交给 provider 补齐展示信息。 */
TZrBool ZrLanguageServer_LspExternalMetadataIdentity_ResolveMember(
        SZrLspMetadataProvider *provider,
        SZrSemanticAnalyzer *analyzer,
        SZrLspProjectIndex *projectIndex,
        const SZrParserSemanticExternalReferenceQuery *identity,
        SZrLspResolvedMetadataMember *outResolved) {
    SZrLspResolvedImportedModule resolved;
    const SZrTypeMemberInfo *match = ZR_NULL;
    TZrSize index;

    if (outResolved != ZR_NULL) {
        memset(outResolved, 0, sizeof(*outResolved));
    }
    if (provider == ZR_NULL || identity == ZR_NULL || outResolved == ZR_NULL ||
        identity->externalOwnerIdentity == ZR_NULL ||
        identity->externalTargetKind == ZR_SEMANTIC_EXTERNAL_TARGET_UNKNOWN ||
        identity->externalMetadataToken == 0U ||
        identity->externalSignatureToken == 0U ||
        identity->externalSignatureHash == 0U ||
        (identity->externalProviderGeneration != 0U &&
         (provider->context == ZR_NULL ||
          identity->externalProviderGeneration !=
                  provider->context->semanticSnapshotProviderGeneration)) ||
        !ZrLanguageServer_LspMetadataProvider_ResolveImportedModule(
                provider,
                analyzer,
                projectIndex,
                identity->externalOwnerIdentity,
                &resolved) ||
        resolved.modulePrototype == ZR_NULL ||
        resolved.modulePrototype->name == ZR_NULL ||
        resolved.modulePrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_MODULE ||
        !ZrCore_String_Equal(
                resolved.modulePrototype->name,
                identity->externalOwnerIdentity) ||
        !resolved.modulePrototype->members.isValid) {
        return ZR_FALSE;
    }

    /* 代次和模块名只限定候选集合；签名令牌才能隔离当前模块内的重载与同名导出。 */
    for (index = 0U; index < resolved.modulePrototype->members.length; index++) {
        const SZrTypeMemberInfo *candidate =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get(
                        (SZrArray *)&resolved.modulePrototype->members, index);

        if (!external_metadata_identity_matches(analyzer, identity, candidate)) {
            continue;
        }
        if (match != ZR_NULL) {
            return ZR_FALSE;
        }
        match = candidate;
    }
    /* BUG: 合法的同名重载可保留不同令牌；若命中第二个重载，provider 按名字取首项，
     * 此处身份复验随即失败，定义与跨快照引用漏掉该目标。生产路径见
     * module_init_analysis.c 的同名函数导出及 type_inference_import_metadata.c 的逐项投影；
     * 用 tests/parser/test_project_import_canonicalization.c 的 pick(true) 第二重载场景扩展 LSP 回归。 */
    if (match == ZR_NULL || match->name == ZR_NULL ||
        !ZrLanguageServer_LspMetadataProvider_ResolveImportedMember(
                provider,
                analyzer,
                projectIndex,
                identity->externalOwnerIdentity,
                match->name,
                outResolved) ||
        !external_metadata_identity_matches(
                analyzer, identity, outResolved->typeMemberInfo)) {
        memset(outResolved, 0, sizeof(*outResolved));
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/* 跨快照引用需要文件位置而不是展示用成员对象；范围来自元数据，URI 由当前 provider 的来源记录指定。 */
TZrBool ZrLanguageServer_LspExternalMetadataIdentity_ResolveDeclaration(
        SZrLspMetadataProvider *provider,
        SZrSemanticAnalyzer *analyzer,
        SZrLspProjectIndex *projectIndex,
        const SZrParserSemanticExternalReferenceQuery *identity,
        SZrLspExternalMetadataIdentityDeclaration *outDeclaration) {
    SZrLspResolvedImportedModule resolved;
    const SZrTypeMemberInfo *match = ZR_NULL;
    TZrSize index;

    if (outDeclaration != ZR_NULL) {
        memset(outDeclaration, 0, sizeof(*outDeclaration));
    }
    if (provider == ZR_NULL || identity == ZR_NULL ||
        outDeclaration == ZR_NULL ||
        identity->externalOwnerIdentity == ZR_NULL ||
        identity->externalTargetKind == ZR_SEMANTIC_EXTERNAL_TARGET_UNKNOWN ||
        identity->externalMetadataToken == 0U ||
        identity->externalSignatureToken == 0U ||
        identity->externalSignatureHash == 0U ||
        (identity->externalProviderGeneration != 0U &&
         (provider->context == ZR_NULL ||
          identity->externalProviderGeneration !=
                  provider->context->semanticSnapshotProviderGeneration)) ||
        !ZrLanguageServer_LspMetadataProvider_ResolveImportedModule(
                provider,
                analyzer,
                projectIndex,
                identity->externalOwnerIdentity,
                &resolved) ||
        resolved.modulePrototype == ZR_NULL ||
        resolved.modulePrototype->name == ZR_NULL ||
        !ZrCore_String_Equal(
                resolved.modulePrototype->name,
                identity->externalOwnerIdentity) ||
        resolved.sourceRecord == ZR_NULL ||
        resolved.sourceRecord->uri == ZR_NULL ||
        !resolved.modulePrototype->members.isValid) {
        return ZR_FALSE;
    }

    /* 不按名称回查声明，以免把同名导出的引用聚合到另一元数据令牌。 */
    for (index = 0U; index < resolved.modulePrototype->members.length; index++) {
        const SZrTypeMemberInfo *candidate =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get(
                        (SZrArray *)&resolved.modulePrototype->members, index);

        if (!external_metadata_identity_matches(analyzer, identity, candidate)) {
            continue;
        }
        if (match != ZR_NULL) {
            return ZR_FALSE;
        }
        match = candidate;
    }
    if (match == ZR_NULL || !match->hasDeclarationRange) {
        return ZR_FALSE;
    }

    outDeclaration->uri = resolved.sourceRecord->uri;
    outDeclaration->range = match->declarationRange;
    outDeclaration->range.source = resolved.sourceRecord->uri;
    outDeclaration->sourceKind = resolved.sourceKind;
    return ZR_TRUE;
}
