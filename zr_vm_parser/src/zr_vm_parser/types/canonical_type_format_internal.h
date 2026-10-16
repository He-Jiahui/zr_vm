#ifndef ZR_VM_PARSER_CANONICAL_TYPE_FORMAT_INTERNAL_H
#define ZR_VM_PARSER_CANONICAL_TYPE_FORMAT_INTERNAL_H

#include "zr_vm_parser/canonical_type.h"

typedef const SZrString *(*TZrCanonicalGenericNameResolver)(
        const SZrSemanticContext *context,
        TZrSymbolId ownerSymbolId,
        TZrUInt32 ordinal,
        EZrCanonicalGenericArgumentKind kind);

/* A supplied resolver must resolve every generic parameter; failure never falls back. */
TZrBool ZrParser_CanonicalType_FormatWithGenericNames(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrChar *buffer,
        TZrSize bufferSize,
        TZrCanonicalGenericNameResolver resolveGenericName);

#endif
