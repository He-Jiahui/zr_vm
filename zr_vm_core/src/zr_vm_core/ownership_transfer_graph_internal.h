#ifndef ZR_VM_CORE_OWNERSHIP_TRANSFER_GRAPH_INTERNAL_H
#define ZR_VM_CORE_OWNERSHIP_TRANSFER_GRAPH_INTERNAL_H

#include "ownership_transfer_cross_domain_internal.h"
#include "zr_vm_core/object.h"

typedef enum EZrDomainTransportValueKind {
    ZR_DOMAIN_TRANSPORT_VALUE_NULL = 0,
    ZR_DOMAIN_TRANSPORT_VALUE_BOOL,
    ZR_DOMAIN_TRANSPORT_VALUE_SIGNED,
    ZR_DOMAIN_TRANSPORT_VALUE_UNSIGNED,
    ZR_DOMAIN_TRANSPORT_VALUE_FLOAT,
    ZR_DOMAIN_TRANSPORT_VALUE_TEXT,
    ZR_DOMAIN_TRANSPORT_VALUE_OBJECT_REF
} EZrDomainTransportValueKind;

typedef struct SZrDomainTransportValue {
    EZrDomainTransportValueKind kind;
    TZrUInt32 valueType;
    TZrUInt32 objectIndex;
    TZrUInt32 textLength;
    TZrUInt64 unsignedValue;
    TZrInt64 signedValue;
    TZrFloat64 floatValue;
    TZrChar *text;
} SZrDomainTransportValue;

typedef struct SZrDomainTransportField {
    SZrDomainTransportValue key;
    SZrDomainTransportValue value;
} SZrDomainTransportField;

typedef struct SZrDomainTransportNode {
    EZrObjectInternalType internalType;
    TZrUInt32 fieldCount;
    SZrDomainTransportField *fields;
} SZrDomainTransportNode;

struct SZrDomainTransferGraph {
    SZrDomainTransportValue root;
    SZrDomainTransportNode *nodes;
    const SZrObject **sourceObjects;
    struct SZrState *sourceState;
    TZrUInt32 nodeCount;
    TZrUInt32 nodeCapacity;
    TZrUInt64 byteCount;
    SZrDomainTransferQuota quota;
};

#endif
