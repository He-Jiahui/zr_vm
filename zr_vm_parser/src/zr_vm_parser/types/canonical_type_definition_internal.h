#ifndef ZR_VM_PARSER_CANONICAL_TYPE_DEFINITION_INTERNAL_H
#define ZR_VM_PARSER_CANONICAL_TYPE_DEFINITION_INTERNAL_H

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"

typedef struct SZrCanonicalTypeDefinitionRecord {
    TZrTypeId typeId;
    TZrSymbolId genericOwnerSymbolId;
    TZrSize genericParameterCount;
    SZrArray genericParameterKinds; // EZrCanonicalGenericArgumentKind
    TZrUInt32 capabilityFlags;
    EZrCanonicalGcScanKind gcScanKind;
    TZrTypeId projectionTypeId;
    SZrArray constructors; // internal constructor records
} SZrCanonicalTypeDefinitionRecord;

SZrCanonicalTypeDefinitionRecord *ZrParser_CanonicalTypeDefinition_FindRecord(
        const SZrSemanticContext *context,
        TZrTypeId typeId);

const SZrCanonicalTypeDefinitionRecord *ZrParser_CanonicalTypeDefinition_ResolveRecord(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        const SZrCanonicalTypeNode **outInstantiation);

#endif // ZR_VM_PARSER_CANONICAL_TYPE_DEFINITION_INTERNAL_H
