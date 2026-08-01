#ifndef ZR_VM_PARSER_COMPILE_TIME_DECLARATION_PATCH_TRANSACTION_H
#define ZR_VM_PARSER_COMPILE_TIME_DECLARATION_PATCH_TRANSACTION_H

#include "compiler_internal.h"
#include "compile_time_declaration_patch_attributes.h"
#include "compile_time_declaration_patch_interfaces.h"
#include "zr_vm_parser/declaration_transform_contract.h"

typedef TZrBool (*FZrParserDeclarationPatchCommitObserver)(
        TZrSize committedAdditionCount,
        TZrPtr userData);

typedef enum EZrParserDeclarationPatchCommitStage {
    ZR_PARSER_DECLARATION_PATCH_COMMIT_GENERATED = 1,
    ZR_PARSER_DECLARATION_PATCH_COMMIT_INTERFACES = 2,
    ZR_PARSER_DECLARATION_PATCH_COMMIT_ATTRIBUTES = 3
} EZrParserDeclarationPatchCommitStage;

typedef TZrBool (*FZrParserDeclarationPatchTransactionObserver)(
        EZrParserDeclarationPatchCommitStage stage,
        TZrSize committedCount,
        TZrPtr userData);

ZR_PARSER_API TZrBool ZrParser_CompileTime_CommitGeneratedFieldsAtomic(
        SZrCompilerState *cs,
        SZrTypePrototypeInfo *targetInfo,
        const SZrParserGeneratedDeclaration *additions,
        SZrString *const *canonicalTypeNames,
        TZrSize additionCount,
        TZrSymbolId originTargetSymbolId,
        SZrFileRange location,
        FZrParserDeclarationPatchCommitObserver observer,
        TZrPtr observerUserData);

ZR_PARSER_API TZrBool ZrParser_CompileTime_CommitDeclarationPatchAtomic(
        SZrCompilerState *cs,
        SZrTypePrototypeInfo *targetInfo,
        const SZrParserGeneratedDeclaration *additions,
        SZrString *const *canonicalTypeNames,
        TZrSize additionCount,
        const SZrParserCompileTimePatchInterfaceAdds *interfaceAdds,
        const SZrParserCompileTimePatchAttributeAdds *attributeAdds,
        SZrString *transformDecoratorName,
        TZrSymbolId originTargetSymbolId,
        SZrFileRange location,
        FZrParserDeclarationPatchTransactionObserver observer,
        TZrPtr observerUserData);

#endif // ZR_VM_PARSER_COMPILE_TIME_DECLARATION_PATCH_TRANSACTION_H
