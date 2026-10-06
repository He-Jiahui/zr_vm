#include "zr_vm_parser/exec_ir_host_aot_target.h"

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/exec_ir_host_primitive_layout.h"
#include "zr_vm_core/hash.h"

#include <string.h>

/* Definition-bound schema 1 widths and finite Win64 C callable ABI facts. */
enum {
    HOST_AOT_OCTET_BITS = 8,
    HOST_AOT_OCTET_MASK = 0xff,
    HOST_AOT_U32_BYTES = 4,
    HOST_AOT_U64_BYTES = 8,
    HOST_AOT_TRIPLE_DOMAIN_BYTES = 22,
    HOST_AOT_TRIPLE_ASCII_BYTES = 22,
    HOST_AOT_ABI_DOMAIN_BYTES = 28,
    HOST_AOT_SCHEMA_VERSION = 1,
    HOST_AOT_POINTER_BYTES = 8,
    HOST_AOT_LITTLE_ENDIAN = 0,
    HOST_AOT_PARAMETER_COUNT = 0,
    HOST_AOT_IMPLICIT_PARAMETER_COUNT = 0,
    HOST_AOT_SIGNED_RETURN_TAG = 1,
    HOST_AOT_RETURN_BITS = 64,
    HOST_AOT_RETURN_BYTES = 8,
    HOST_AOT_RETURN_ALIGN = 8,
    HOST_AOT_WIN64_DIRECT_I64_RAX_RULE = 1,
    HOST_AOT_REQUIRED_CAPABILITIES = 0,
    HOST_AOT_TRIPLE_SCHEMA_OFFSET = HOST_AOT_TRIPLE_DOMAIN_BYTES,
    HOST_AOT_TRIPLE_LENGTH_OFFSET = HOST_AOT_TRIPLE_SCHEMA_OFFSET + HOST_AOT_U32_BYTES,
    HOST_AOT_TRIPLE_VALUE_OFFSET = HOST_AOT_TRIPLE_LENGTH_OFFSET + HOST_AOT_U32_BYTES,
    HOST_AOT_TRIPLE_HASH_BYTES = HOST_AOT_TRIPLE_VALUE_OFFSET + HOST_AOT_TRIPLE_ASCII_BYTES,
    HOST_AOT_ABI_SCHEMA_OFFSET = HOST_AOT_ABI_DOMAIN_BYTES,
    HOST_AOT_ABI_VERSION_OFFSET = HOST_AOT_ABI_SCHEMA_OFFSET + HOST_AOT_U32_BYTES,
    HOST_AOT_ABI_TRIPLE_OFFSET = HOST_AOT_ABI_VERSION_OFFSET + HOST_AOT_U32_BYTES,
    HOST_AOT_ABI_FACTS_OFFSET = HOST_AOT_ABI_TRIPLE_OFFSET + HOST_AOT_U64_BYTES,
    HOST_AOT_ABI_FACT_COUNT = 11,
    HOST_AOT_ABI_HASH_BYTES = HOST_AOT_ABI_FACTS_OFFSET +
            HOST_AOT_ABI_FACT_COUNT * HOST_AOT_U32_BYTES
};

/* Explicit ASCII bytes exclude execution-character-set and terminating-NUL data. */
static const TZrByte host_aot_triple_domain[HOST_AOT_TRIPLE_DOMAIN_BYTES] = {
    0x7a, 0x72, 0x2e, 0x61, 0x6f, 0x74, 0x69, 0x72, 0x2e, 0x74, 0x61,
    0x72, 0x67, 0x65, 0x74, 0x2d, 0x74, 0x72, 0x69, 0x70, 0x6c, 0x65
};
static const TZrByte host_aot_triple_ascii[HOST_AOT_TRIPLE_ASCII_BYTES] = {
    0x78, 0x38, 0x36, 0x5f, 0x36, 0x34, 0x2d, 0x70, 0x63, 0x2d, 0x77,
    0x69, 0x6e, 0x64, 0x6f, 0x77, 0x73, 0x2d, 0x6d, 0x73, 0x76, 0x63
};
static const TZrByte host_aot_abi_domain[HOST_AOT_ABI_DOMAIN_BYTES] = {
    0x7a, 0x72, 0x2e, 0x61, 0x6f, 0x74, 0x69, 0x72, 0x2e, 0x68, 0x6f,
    0x73, 0x74, 0x2e, 0x6e, 0x6f, 0x61, 0x72, 0x67, 0x73, 0x2d, 0x69,
    0x36, 0x34, 0x2e, 0x61, 0x62, 0x69
};

static TZrBool host_aot_fail(SZrAotIrDiagnostic *diagnostic, EZrAotIrStatus status,
                           TZrTypeId callableTypeId, TZrUInt64 expected,
                           TZrUInt64 actual) {
    if (diagnostic != ZR_NULL) {
        diagnostic->status = status;
        diagnostic->index = callableTypeId;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
    }
    return ZR_FALSE;
}

/* Numeric preflight does not establish readable storage; the caller owns that
 * precondition and the sorted, current interner snapshot throughout this call. */
static TZrBool host_aot_canonical_preflight(const SZrSemanticContext *context,
                                          TZrTypeId callableTypeId,
                                          SZrAotIrDiagnostic *diagnostic) {
    const SZrArray *types = &context->canonicalTypes;
    TZrSize span;
    uintptr_t start;

    if (!types->isValid)
        return host_aot_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE, callableTypeId,
                             ZR_TRUE, types->isValid);
    if (types->elementSize != sizeof(SZrCanonicalTypeNode))
        return host_aot_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE, callableTypeId,
                             sizeof(SZrCanonicalTypeNode), types->elementSize);
    if (types->length > types->capacity)
        return host_aot_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE, callableTypeId,
                             types->capacity, types->length);
    if (types->capacity != 0U && types->head == ZR_NULL)
        return host_aot_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE, callableTypeId,
                             ZR_TRUE, ZR_FALSE);
    if (types->capacity > SIZE_MAX / sizeof(SZrCanonicalTypeNode))
        return host_aot_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE, callableTypeId,
                             SIZE_MAX / sizeof(SZrCanonicalTypeNode), types->capacity);
    span = types->capacity * sizeof(SZrCanonicalTypeNode);
    start = (uintptr_t)types->head;
    if ((uintmax_t)span > (uintmax_t)UINTPTR_MAX)
        return host_aot_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE, callableTypeId,
                             UINTPTR_MAX, span);
    if (start > UINTPTR_MAX - (uintptr_t)span)
        return host_aot_fail(diagnostic, ZR_AOT_IR_INVALID_RANGE, callableTypeId,
                             UINTPTR_MAX - (uintptr_t)span, start);
    return ZR_TRUE;
}

static TZrBool host_aot_host_supported(TZrTypeId callableTypeId,
                                     SZrAotIrDiagnostic *diagnostic) {
#if defined(_WIN32) && defined(_WIN64) && defined(_MSC_VER) && \
        (defined(_M_X64) || defined(_M_AMD64)) && \
        !defined(_M_ARM64) && !defined(_M_ARM64EC)
    const TZrUInt64 pattern = UINT64_C(0x0102030405060708);
    const TZrByte little[HOST_AOT_U64_BYTES] = {8, 7, 6, 5, 4, 3, 2, 1};
    TZrByte observed[HOST_AOT_U64_BYTES];
    TZrUInt32 hostCharBits = CHAR_BIT;
    TZrSize hostPointerBytes = sizeof(void *);
    TZrSize hostReturnBytes = sizeof(TZrInt64);
    TZrSize hostReturnAlign = alignof(TZrInt64);
    TZrSize patternBytes = sizeof(pattern);

    if (hostCharBits != HOST_AOT_OCTET_BITS)
        return host_aot_fail(diagnostic, ZR_AOT_IR_UNSUPPORTED, callableTypeId,
                             HOST_AOT_OCTET_BITS, hostCharBits);
    if (hostPointerBytes != HOST_AOT_POINTER_BYTES)
        return host_aot_fail(diagnostic, ZR_AOT_IR_UNSUPPORTED, callableTypeId,
                             HOST_AOT_POINTER_BYTES, hostPointerBytes);
    if (hostReturnBytes != HOST_AOT_RETURN_BYTES)
        return host_aot_fail(diagnostic, ZR_AOT_IR_UNSUPPORTED, callableTypeId,
                             HOST_AOT_RETURN_BYTES, hostReturnBytes);
    if (hostReturnAlign != HOST_AOT_RETURN_ALIGN)
        return host_aot_fail(diagnostic, ZR_AOT_IR_UNSUPPORTED, callableTypeId,
                             HOST_AOT_RETURN_ALIGN, hostReturnAlign);
    if (patternBytes != sizeof(observed))
        return host_aot_fail(diagnostic, ZR_AOT_IR_UNSUPPORTED, callableTypeId,
                             sizeof(observed), patternBytes);
    memcpy(observed, &pattern, sizeof(observed));
    if (memcmp(observed, little, sizeof(observed)) != 0)
        return host_aot_fail(diagnostic, ZR_AOT_IR_UNSUPPORTED, callableTypeId,
                             ZR_TRUE, ZR_FALSE);
    return ZR_TRUE;
#else
    return host_aot_fail(diagnostic, ZR_AOT_IR_UNSUPPORTED, callableTypeId,
                         ZR_TRUE, ZR_FALSE);
#endif
}

static void host_aot_encode_le(TZrByte *bytes, TZrUInt64 value, TZrSize width) {
    TZrSize index;
    for (index = 0U; index < width; ++index) {
        bytes[index] = (TZrByte)(value & HOST_AOT_OCTET_MASK);
        value >>= HOST_AOT_OCTET_BITS;
    }
}

static TZrUInt64 host_aot_triple_hash(void) {
    TZrByte bytes[HOST_AOT_TRIPLE_HASH_BYTES];
    memcpy(bytes, host_aot_triple_domain, sizeof(host_aot_triple_domain));
    host_aot_encode_le(bytes + HOST_AOT_TRIPLE_SCHEMA_OFFSET,
                       HOST_AOT_SCHEMA_VERSION, HOST_AOT_U32_BYTES);
    host_aot_encode_le(bytes + HOST_AOT_TRIPLE_LENGTH_OFFSET,
                       HOST_AOT_TRIPLE_ASCII_BYTES, HOST_AOT_U32_BYTES);
    memcpy(bytes + HOST_AOT_TRIPLE_VALUE_OFFSET, host_aot_triple_ascii,
           sizeof(host_aot_triple_ascii));
    return ZrCore_Hash_CreateStable64(bytes, sizeof(bytes));
}

static TZrUInt64 host_aot_abi_hash(const SZrAotIrTargetContract *candidate) {
    TZrByte bytes[HOST_AOT_ABI_HASH_BYTES];
    const TZrUInt32 facts[HOST_AOT_ABI_FACT_COUNT] = {
        candidate->pointerSize, CHAR_BIT, candidate->endianness,
        ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64,
        HOST_AOT_PARAMETER_COUNT, HOST_AOT_IMPLICIT_PARAMETER_COUNT,
        HOST_AOT_SIGNED_RETURN_TAG, HOST_AOT_RETURN_BITS,
        HOST_AOT_RETURN_BYTES, HOST_AOT_RETURN_ALIGN,
        HOST_AOT_WIN64_DIRECT_I64_RAX_RULE
    };
    TZrSize index;

    memcpy(bytes, host_aot_abi_domain, sizeof(host_aot_abi_domain));
    host_aot_encode_le(bytes + HOST_AOT_ABI_SCHEMA_OFFSET,
                       HOST_AOT_SCHEMA_VERSION, HOST_AOT_U32_BYTES);
    host_aot_encode_le(bytes + HOST_AOT_ABI_VERSION_OFFSET,
                       candidate->abiVersion, HOST_AOT_U32_BYTES);
    host_aot_encode_le(bytes + HOST_AOT_ABI_TRIPLE_OFFSET,
                       candidate->targetTripleHash, HOST_AOT_U64_BYTES);
    for (index = 0U; index < HOST_AOT_ABI_FACT_COUNT; ++index)
        host_aot_encode_le(bytes + HOST_AOT_ABI_FACTS_OFFSET + index * HOST_AOT_U32_BYTES,
                           facts[index], HOST_AOT_U32_BYTES);
    return ZrCore_Hash_CreateStable64(bytes, sizeof(bytes));
}

TZrBool ZrParser_ExecIr_MakeHostNoArgsI64AotTarget(
        const SZrSemanticContext *context, TZrTypeId callableTypeId,
        const SZrExecIrLayout *returnLayout, SZrAotIrTargetContract *output,
        SZrAotIrDiagnostic *diagnostic) {
    const SZrCanonicalTypeNode *callable;
    const SZrCanonicalTypeNode *returnNode;
    SZrExecIrLayout actualLayout;
    SZrAotIrTargetContract candidate;
    TZrTypeId returnTypeId;

    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (context == ZR_NULL || returnLayout == ZR_NULL || output == ZR_NULL)
        return host_aot_fail(diagnostic, ZR_AOT_IR_INVALID_ARGUMENT, callableTypeId,
                             ZR_TRUE, ZR_FALSE);
    if (callableTypeId == ZR_SEMANTIC_ID_INVALID)
        return host_aot_fail(diagnostic, ZR_AOT_IR_INVALID_ID, callableTypeId,
                             ZR_TRUE, ZR_FALSE);
    if (!host_aot_canonical_preflight(context, callableTypeId, diagnostic))
        return ZR_FALSE;

    callable = ZrParser_CanonicalType_Find(context, callableTypeId);
    if (callable == ZR_NULL || callable->id != callableTypeId)
        return host_aot_fail(diagnostic, ZR_AOT_IR_INVALID_ID, callableTypeId,
                             callableTypeId, callable != ZR_NULL ? callable->id : 0U);
    if (callable->kind != ZR_CANONICAL_TYPE_FUNCTION)
        return host_aot_fail(diagnostic, ZR_AOT_IR_INVALID_SIGNATURE, callableTypeId,
                             ZR_CANONICAL_TYPE_FUNCTION, callable->kind);
    if (callable->structuralHash == 0U)
        return host_aot_fail(diagnostic, ZR_AOT_IR_INVALID_SIGNATURE, callableTypeId,
                             ZR_TRUE, callable->structuralHash);
    if (callable->data.function.parameterContracts.length != HOST_AOT_PARAMETER_COUNT)
        return host_aot_fail(diagnostic, ZR_AOT_IR_UNSUPPORTED, callableTypeId,
                             HOST_AOT_PARAMETER_COUNT,
                             callable->data.function.parameterContracts.length);
    if (callable->data.function.receiverEffect != ZR_CANONICAL_RECEIVER_NONE)
        return host_aot_fail(diagnostic, ZR_AOT_IR_UNSUPPORTED, callableTypeId,
                             ZR_CANONICAL_RECEIVER_NONE, callable->data.function.receiverEffect);
    if (callable->data.function.effectFlags != ZR_CANONICAL_CALLABLE_EFFECT_NONE)
        return host_aot_fail(diagnostic, ZR_AOT_IR_UNSUPPORTED, callableTypeId,
                             ZR_CANONICAL_CALLABLE_EFFECT_NONE, callable->data.function.effectFlags);

    returnTypeId = callable->data.function.returnTypeId;
    returnNode = ZrParser_CanonicalType_Find(context, returnTypeId);
    if (returnNode == ZR_NULL || returnNode->id != returnTypeId)
        return host_aot_fail(diagnostic, ZR_AOT_IR_UNSUPPORTED, callableTypeId,
                             returnTypeId, returnNode != ZR_NULL ? returnNode->id : 0U);
    if (returnNode->kind != ZR_CANONICAL_TYPE_PRIMITIVE)
        return host_aot_fail(diagnostic, ZR_AOT_IR_UNSUPPORTED, callableTypeId,
                             ZR_CANONICAL_TYPE_PRIMITIVE, returnNode->kind);
    if (returnNode->data.primitive.valueType != ZR_VALUE_TYPE_INT64)
        return host_aot_fail(diagnostic, ZR_AOT_IR_UNSUPPORTED, callableTypeId,
                             ZR_VALUE_TYPE_INT64, returnNode->data.primitive.valueType);
    if (!host_aot_host_supported(callableTypeId, diagnostic)) return ZR_FALSE;

    if (returnLayout->id == ZR_AOT_IR_ID_INVALID)
        return host_aot_fail(diagnostic, ZR_AOT_IR_INVALID_LAYOUT, callableTypeId,
                             ZR_TRUE, returnLayout->id);
    if (returnLayout->typeToken != returnTypeId)
        return host_aot_fail(diagnostic, ZR_AOT_IR_INVALID_LAYOUT, callableTypeId,
                             returnTypeId, returnLayout->typeToken);
    memset(&actualLayout, 0, sizeof(actualLayout));
    if (!ZrParser_ExecIr_MakeHostPrimitiveLayout(context, returnTypeId, returnLayout->id,
                                               &actualLayout, ZR_NULL))
        return host_aot_fail(diagnostic, ZR_AOT_IR_INVALID_LAYOUT, callableTypeId,
                             ZR_TRUE, ZR_FALSE);
    if (returnLayout->id != actualLayout.id)
        return host_aot_fail(diagnostic, ZR_AOT_IR_INVALID_LAYOUT, callableTypeId,
                             actualLayout.id, returnLayout->id);
    if (returnLayout->typeToken != actualLayout.typeToken)
        return host_aot_fail(diagnostic, ZR_AOT_IR_INVALID_LAYOUT, callableTypeId,
                             actualLayout.typeToken, returnLayout->typeToken);
    if (returnLayout->byteSize != actualLayout.byteSize)
        return host_aot_fail(diagnostic, ZR_AOT_IR_INVALID_LAYOUT, callableTypeId,
                             actualLayout.byteSize, returnLayout->byteSize);
    if (returnLayout->byteAlign != actualLayout.byteAlign)
        return host_aot_fail(diagnostic, ZR_AOT_IR_INVALID_LAYOUT, callableTypeId,
                             actualLayout.byteAlign, returnLayout->byteAlign);
    if (returnLayout->layoutHash != actualLayout.layoutHash)
        return host_aot_fail(diagnostic, ZR_AOT_IR_INVALID_LAYOUT, callableTypeId,
                             actualLayout.layoutHash, returnLayout->layoutHash);

    memset(&candidate, 0, sizeof(candidate));
    candidate.abiVersion = ZR_AOT_IR_TARGET_ABI_VERSION;
    candidate.pointerSize = HOST_AOT_POINTER_BYTES;
    candidate.endianness = HOST_AOT_LITTLE_ENDIAN;
    candidate.requiredCapabilities = HOST_AOT_REQUIRED_CAPABILITIES;
    candidate.targetTripleHash = host_aot_triple_hash();
    if (candidate.targetTripleHash == 0U)
        return host_aot_fail(diagnostic, ZR_AOT_IR_INVALID_TARGET, callableTypeId,
                             ZR_TRUE, candidate.targetTripleHash);
    candidate.abiHash = host_aot_abi_hash(&candidate);
    if (candidate.abiHash == 0U)
        return host_aot_fail(diagnostic, ZR_AOT_IR_INVALID_TARGET, callableTypeId,
                             ZR_TRUE, candidate.abiHash);
    if (ZrCore_AotIr_ValidateTarget(&candidate, diagnostic) != ZR_AOT_IR_OK)
        return ZR_FALSE;

    memcpy(output, &candidate, sizeof(candidate));
    return ZR_TRUE;
}
