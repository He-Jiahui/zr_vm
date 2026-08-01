//
// Canonical immutable declaration views and append-only transform patches.
//

#ifndef ZR_VM_PARSER_DECLARATION_TRANSFORM_CONTRACT_H
#define ZR_VM_PARSER_DECLARATION_TRANSFORM_CONTRACT_H

#include "zr_vm_parser/attribute_contract.h"
#include "zr_vm_parser/compile_tool.h"

#define ZR_PARSER_DECLARATION_TRANSFORM_MAX_ADDITIONS ((TZrSize)10000U)

typedef enum EZrParserDeclarationKind {
    ZR_PARSER_DECLARATION_KIND_INVALID = 0,
    ZR_PARSER_DECLARATION_KIND_TYPE = 1,
    ZR_PARSER_DECLARATION_KIND_FIELD = 2,
    ZR_PARSER_DECLARATION_KIND_METHOD = 3,
    ZR_PARSER_DECLARATION_KIND_PROPERTY = 4,
    ZR_PARSER_DECLARATION_KIND_FUNCTION = 5,
    ZR_PARSER_DECLARATION_KIND_PARAMETER = 6
} EZrParserDeclarationKind;

typedef enum EZrParserGeneratedDeclarationKind {
    ZR_PARSER_GENERATED_DECLARATION_FIELD = 2
} EZrParserGeneratedDeclarationKind;

typedef enum EZrParserGeneratedVisibility {
    ZR_PARSER_GENERATED_VISIBILITY_PRIVATE = 0,
    ZR_PARSER_GENERATED_VISIBILITY_PROTECTED = 1,
    ZR_PARSER_GENERATED_VISIBILITY_PUBLIC = 2
} EZrParserGeneratedVisibility;

typedef enum EZrParserGeneratedMutability {
    ZR_PARSER_GENERATED_MUTABILITY_LET = 0,
    ZR_PARSER_GENERATED_MUTABILITY_VAR = 1
} EZrParserGeneratedMutability;

typedef struct SZrParserDeclarationView {
    TZrSymbolId symbolId;
    EZrParserDeclarationKind kind;
    const TZrChar *name;
    TZrSymbolId ownerSymbolId;
    SZrFileRange sourceRange;
    const SZrParserAttributeData *attributes;
    TZrSize attributeCount;
    const TZrChar *const *existingMemberNames;
    TZrSize existingMemberCount;
} SZrParserDeclarationView;

typedef struct SZrParserGeneratedDeclaration {
    EZrParserGeneratedDeclarationKind kind;
    const TZrChar *name;
    TZrTypeId typeId;
    EZrParserGeneratedVisibility visibility;
    EZrParserGeneratedMutability mutability;
    TZrBool hasConstantInitializer;
    SZrParserAttributeConstant constantInitializer;
    const SZrParserAttributeData *attributes;
    TZrSize attributeCount;
} SZrParserGeneratedDeclaration;

typedef struct SZrParserCompileDiagnostic {
    TZrBool isError;
    const TZrChar *message;
    TZrSymbolId targetSymbolId;
} SZrParserCompileDiagnostic;

typedef struct SZrParserDeclarationPatch {
    TZrSymbolId targetSymbolId;
    const SZrParserGeneratedDeclaration *additions;
    TZrSize additionCount;
    const TZrTypeId *interfaceAdds;
    TZrSize interfaceAddCount;
    const SZrParserAttributeData *attributeAdds;
    TZrSize attributeAddCount;
    const SZrParserCompileDiagnostic *diagnostics;
    TZrSize diagnosticCount;
    TZrUInt32 expansionRound;
} SZrParserDeclarationPatch;

typedef enum EZrParserDeclarationPatchError {
    ZR_PARSER_DECLARATION_PATCH_VALID = 0,
    ZR_PARSER_DECLARATION_PATCH_ERROR_ARGUMENT,
    ZR_PARSER_DECLARATION_PATCH_ERROR_TARGET,
    ZR_PARSER_DECLARATION_PATCH_ERROR_ROUND,
    ZR_PARSER_DECLARATION_PATCH_ERROR_BUDGET,
    ZR_PARSER_DECLARATION_PATCH_ERROR_KIND,
    ZR_PARSER_DECLARATION_PATCH_ERROR_NAME,
    ZR_PARSER_DECLARATION_PATCH_ERROR_COLLISION,
    ZR_PARSER_DECLARATION_PATCH_ERROR_TYPE,
    ZR_PARSER_DECLARATION_PATCH_ERROR_VISIBILITY,
    ZR_PARSER_DECLARATION_PATCH_ERROR_MUTABILITY,
    ZR_PARSER_DECLARATION_PATCH_ERROR_RECURSIVE_TRANSFORM,
    ZR_PARSER_DECLARATION_PATCH_ERROR_PHASE_CYCLE
} EZrParserDeclarationPatchError;

ZR_PARSER_API EZrParserDeclarationPatchError ZrParser_DeclarationPatch_Validate(
        const SZrParserDeclarationView *view,
        const SZrParserDeclarationPatch *patch);
ZR_PARSER_API EZrParserDeclarationPatchError ZrParser_DeclarationView_ValidatePhaseAccess(
        EZrParserCompilePhase currentPhase,
        EZrParserCompilePhase minimumFieldPhase);
ZR_PARSER_API const TZrChar *ZrParser_DeclarationPatch_ErrorName(
        EZrParserDeclarationPatchError error);

#endif // ZR_VM_PARSER_DECLARATION_TRANSFORM_CONTRACT_H
