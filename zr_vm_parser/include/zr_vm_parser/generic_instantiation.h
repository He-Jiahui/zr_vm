#ifndef ZR_VM_PARSER_GENERIC_INSTANTIATION_H
#define ZR_VM_PARSER_GENERIC_INSTANTIATION_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/type_system.h"

typedef enum EZrGenericInstantiationShareKind {
    ZR_GENERIC_INSTANTIATION_SHARE_KIND_MONOMORPHIZED_VALUE = 0,
    ZR_GENERIC_INSTANTIATION_SHARE_KIND_SHARED_REFERENCE = 1
} EZrGenericInstantiationShareKind;

typedef enum EZrGenericInstantiationTypeShape {
    ZR_GENERIC_INSTANTIATION_TYPE_SHAPE_VALUE = 0,
    ZR_GENERIC_INSTANTIATION_TYPE_SHAPE_REFERENCE = 1
} EZrGenericInstantiationTypeShape;

typedef struct SZrGenericInstantiationTypeArgument {
    SZrInferredType type;
    EZrGenericInstantiationTypeShape shape;
} SZrGenericInstantiationTypeArgument;

typedef struct SZrGenericInstantiationRecord {
    TZrUInt32 baseToken;
    SZrArray arguments; // SZrGenericInstantiationTypeArgument
    EZrGenericInstantiationShareKind shareKind;
    TZrUInt32 cInstanceId;
} SZrGenericInstantiationRecord;

typedef struct SZrGenericInstantiationTable {
    SZrArray records; // SZrGenericInstantiationRecord
    TZrUInt32 nextCInstanceId;
} SZrGenericInstantiationTable;

ZR_PARSER_API EZrGenericInstantiationTypeShape ZrParser_GenericInstantiation_InferTypeShape(
        const SZrInferredType *type);

ZR_PARSER_API EZrGenericInstantiationShareKind ZrParser_GenericInstantiation_ComputeShareKind(
        const SZrGenericInstantiationTypeArgument *arguments,
        TZrSize argumentCount);

ZR_PARSER_API void ZrParser_GenericInstantiationTable_Init(SZrState *state,
                                                           SZrGenericInstantiationTable *table);

ZR_PARSER_API void ZrParser_GenericInstantiationTable_Free(SZrState *state,
                                                           SZrGenericInstantiationTable *table);

ZR_PARSER_API TZrSize ZrParser_GenericInstantiationTable_Count(const SZrGenericInstantiationTable *table);

ZR_PARSER_API const SZrGenericInstantiationRecord *ZrParser_GenericInstantiationTable_At(
        const SZrGenericInstantiationTable *table,
        TZrSize index);

ZR_PARSER_API TZrBool ZrParser_GenericInstantiationTable_GetOrAdd(
        SZrState *state,
        SZrGenericInstantiationTable *table,
        TZrUInt32 baseToken,
        const SZrInferredType *argumentTypes,
        TZrSize argumentCount,
        const SZrGenericInstantiationRecord **outRecord);

ZR_PARSER_API TZrBool ZrParser_GenericInstantiationTable_GetOrAddResolved(
        SZrState *state,
        SZrGenericInstantiationTable *table,
        TZrUInt32 baseToken,
        const SZrGenericInstantiationTypeArgument *arguments,
        TZrSize argumentCount,
        const SZrGenericInstantiationRecord **outRecord);

#endif // ZR_VM_PARSER_GENERIC_INSTANTIATION_H
