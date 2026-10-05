#include "zr_vm_parser/exec_ir_host_primitive_layout.h"

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_core/hash.h"

#include <string.h>

/* Schema 1 has a contiguous domain and fixed-width little-endian payload. */
enum {
    HOST_LAYOUT_OCTET_BITS = 8,
    HOST_LAYOUT_U32_BYTES = 4,
    HOST_LAYOUT_U64_BYTES = 8,
    HOST_LAYOUT_DOMAIN_BYTES = 31,
    HOST_LAYOUT_SCHEMA_OFFSET = HOST_LAYOUT_DOMAIN_BYTES,
    HOST_LAYOUT_PRIMITIVE_OFFSET = HOST_LAYOUT_SCHEMA_OFFSET + HOST_LAYOUT_U32_BYTES,
    HOST_LAYOUT_STRUCTURAL_HASH_OFFSET = HOST_LAYOUT_PRIMITIVE_OFFSET + HOST_LAYOUT_U32_BYTES,
    HOST_LAYOUT_SIZE_OFFSET = HOST_LAYOUT_STRUCTURAL_HASH_OFFSET + HOST_LAYOUT_U64_BYTES,
    HOST_LAYOUT_ALIGN_OFFSET = HOST_LAYOUT_SIZE_OFFSET + HOST_LAYOUT_U32_BYTES,
    HOST_LAYOUT_CHAR_BIT_OFFSET = HOST_LAYOUT_ALIGN_OFFSET + HOST_LAYOUT_U32_BYTES,
    HOST_LAYOUT_BYTE_ORDER_OFFSET = HOST_LAYOUT_CHAR_BIT_OFFSET + HOST_LAYOUT_U32_BYTES,
    HOST_LAYOUT_HASH_BYTES = HOST_LAYOUT_BYTE_ORDER_OFFSET + HOST_LAYOUT_U32_BYTES,
    HOST_LAYOUT_SCHEMA_VERSION = 1,
    HOST_LAYOUT_LITTLE_ENDIAN = 0,
    HOST_LAYOUT_BIG_ENDIAN = 1
};

/* Explicit ASCII bytes also avoid depending on the execution character set. */
static const TZrByte host_layout_domain[HOST_LAYOUT_DOMAIN_BYTES] = {
    0x7a, 0x72, 0x2e, 0x65, 0x78, 0x65, 0x63, 0x69, 0x72, 0x2e,
    0x68, 0x6f, 0x73, 0x74, 0x2e, 0x70, 0x72, 0x69, 0x6d, 0x69,
    0x74, 0x69, 0x76, 0x65, 0x2d, 0x6c, 0x61, 0x79, 0x6f, 0x75, 0x74
};

static TZrBool host_layout_fail(SZrExecIrDiagnostic *diagnostic,
                               EZrExecutionDiagnosticCode code) {
    if (diagnostic != ZR_NULL) diagnostic->code = code;
    return ZR_FALSE;
}

static void host_layout_encode_le(TZrByte *bytes, TZrUInt64 value, TZrSize width) {
    TZrSize index;
    for (index = 0; index < width; index++) {
        bytes[index] = (TZrByte)(value & 0xffU);
        value >>= HOST_LAYOUT_OCTET_BITS;
    }
}

static TZrBool host_layout_byte_order(TZrUInt32 *byteOrder) {
    const TZrUInt64 pattern = UINT64_C(0x0102030405060708);
    const TZrByte little[HOST_LAYOUT_U64_BYTES] = {8, 7, 6, 5, 4, 3, 2, 1};
    const TZrByte big[HOST_LAYOUT_U64_BYTES] = {1, 2, 3, 4, 5, 6, 7, 8};
    TZrByte bytes[HOST_LAYOUT_U64_BYTES];

    if (CHAR_BIT != HOST_LAYOUT_OCTET_BITS || sizeof(pattern) != sizeof(bytes))
        return ZR_FALSE;
    memcpy(bytes, &pattern, sizeof(bytes));
    if (memcmp(bytes, little, sizeof(bytes)) == 0) {
        *byteOrder = HOST_LAYOUT_LITTLE_ENDIAN;
        return ZR_TRUE;
    }
    if (memcmp(bytes, big, sizeof(bytes)) == 0) {
        *byteOrder = HOST_LAYOUT_BIG_ENDIAN;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

TZrBool ZrParser_ExecIr_MakeHostPrimitiveLayout(
        const SZrSemanticContext *context, TZrTypeId typeId, TZrUInt32 layoutId,
        SZrExecIrLayout *output, SZrExecIrDiagnostic *diagnostic) {
    const SZrArray *types;
    const SZrCanonicalTypeNode *node;
    TZrSize span;
    uintptr_t start;
    TZrUInt32 byteOrder;
    TZrByte hashBytes[HOST_LAYOUT_HASH_BYTES];
    SZrExecIrLayout candidate;

    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
    if (context == ZR_NULL || output == ZR_NULL || typeId == 0U || layoutId == 0U)
        return host_layout_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);

    types = &context->canonicalTypes;
    if (!types->isValid || types->elementSize != sizeof(SZrCanonicalTypeNode) ||
        types->length > types->capacity ||
        (types->capacity != 0U && types->head == ZR_NULL))
        return host_layout_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
    if (types->capacity > SIZE_MAX / sizeof(SZrCanonicalTypeNode))
        return host_layout_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
    span = types->capacity * sizeof(SZrCanonicalTypeNode);
    start = (uintptr_t)types->head;
    if ((uintmax_t)span > (uintmax_t)UINTPTR_MAX ||
        start > UINTPTR_MAX - (uintptr_t)span)
        return host_layout_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);

    node = ZrParser_CanonicalType_Find(context, typeId);
    if (node == ZR_NULL || node->id != typeId)
        return host_layout_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
    if (node->kind != ZR_CANONICAL_TYPE_PRIMITIVE ||
        node->data.primitive.valueType != ZR_VALUE_TYPE_INT64)
        return host_layout_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    if (node->structuralHash == 0U)
        return host_layout_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH);
    if ((uintmax_t)sizeof(TZrInt64) > (uintmax_t)UINT32_MAX ||
        (uintmax_t)alignof(TZrInt64) > (uintmax_t)UINT32_MAX)
        return host_layout_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
    if (!host_layout_byte_order(&byteOrder))
        return host_layout_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);

    memset(&candidate, 0, sizeof(candidate));
    candidate.id = layoutId;
    candidate.typeToken = typeId;
    candidate.byteSize = (TZrUInt32)sizeof(TZrInt64);
    candidate.byteAlign = (TZrUInt32)alignof(TZrInt64);
    memcpy(hashBytes, host_layout_domain, sizeof(host_layout_domain));
    host_layout_encode_le(hashBytes + HOST_LAYOUT_SCHEMA_OFFSET,
                          HOST_LAYOUT_SCHEMA_VERSION, HOST_LAYOUT_U32_BYTES);
    host_layout_encode_le(hashBytes + HOST_LAYOUT_PRIMITIVE_OFFSET,
                          (TZrUInt32)node->data.primitive.valueType, HOST_LAYOUT_U32_BYTES);
    host_layout_encode_le(hashBytes + HOST_LAYOUT_STRUCTURAL_HASH_OFFSET,
                          node->structuralHash, HOST_LAYOUT_U64_BYTES);
    host_layout_encode_le(hashBytes + HOST_LAYOUT_SIZE_OFFSET,
                          candidate.byteSize, HOST_LAYOUT_U32_BYTES);
    host_layout_encode_le(hashBytes + HOST_LAYOUT_ALIGN_OFFSET,
                          candidate.byteAlign, HOST_LAYOUT_U32_BYTES);
    host_layout_encode_le(hashBytes + HOST_LAYOUT_CHAR_BIT_OFFSET,
                          CHAR_BIT, HOST_LAYOUT_U32_BYTES);
    host_layout_encode_le(hashBytes + HOST_LAYOUT_BYTE_ORDER_OFFSET,
                          byteOrder, HOST_LAYOUT_U32_BYTES);
    candidate.layoutHash = ZrCore_Hash_CreateStable64(hashBytes, sizeof(hashBytes));
    if (candidate.layoutHash == 0U)
        return host_layout_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH);

    memcpy(output, &candidate, sizeof(candidate));
    return ZR_TRUE;
}
