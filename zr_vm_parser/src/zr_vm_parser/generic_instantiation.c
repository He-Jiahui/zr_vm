#include "zr_vm_parser/generic_instantiation.h"

static TZrBool generic_instantiation_shape_is_reference(EZrGenericInstantiationTypeShape shape) {
    return shape == ZR_GENERIC_INSTANTIATION_TYPE_SHAPE_REFERENCE;
}

EZrGenericInstantiationTypeShape ZrParser_GenericInstantiation_InferTypeShape(const SZrInferredType *type) {
    if (type == ZR_NULL) {
        return ZR_GENERIC_INSTANTIATION_TYPE_SHAPE_VALUE;
    }

    switch (type->baseType) {
        case ZR_VALUE_TYPE_STRING:
        case ZR_VALUE_TYPE_BUFFER:
        case ZR_VALUE_TYPE_ARRAY:
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE_VALUE:
        case ZR_VALUE_TYPE_CLOSURE:
        case ZR_VALUE_TYPE_OBJECT:
        case ZR_VALUE_TYPE_THREAD:
            return ZR_GENERIC_INSTANTIATION_TYPE_SHAPE_REFERENCE;
        default:
            return ZR_GENERIC_INSTANTIATION_TYPE_SHAPE_VALUE;
    }
}

EZrGenericInstantiationShareKind ZrParser_GenericInstantiation_ComputeShareKind(
        const SZrGenericInstantiationTypeArgument *arguments,
        TZrSize argumentCount) {
    if (arguments == ZR_NULL || argumentCount == 0) {
        return ZR_GENERIC_INSTANTIATION_SHARE_KIND_MONOMORPHIZED_VALUE;
    }

    for (TZrSize i = 0; i < argumentCount; ++i) {
        if (!generic_instantiation_shape_is_reference(arguments[i].shape)) {
            return ZR_GENERIC_INSTANTIATION_SHARE_KIND_MONOMORPHIZED_VALUE;
        }
    }

    return ZR_GENERIC_INSTANTIATION_SHARE_KIND_SHARED_REFERENCE;
}

void ZrParser_GenericInstantiationTable_Init(SZrState *state, SZrGenericInstantiationTable *table) {
    if (state == ZR_NULL || table == ZR_NULL) {
        return;
    }

    ZrCore_Array_Init(state, &table->records, sizeof(SZrGenericInstantiationRecord), 4u);
    table->nextCInstanceId = 1u;
}

static void generic_instantiation_argument_free(SZrState *state, SZrGenericInstantiationTypeArgument *argument) {
    if (argument == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Free(state, &argument->type);
    argument->shape = ZR_GENERIC_INSTANTIATION_TYPE_SHAPE_VALUE;
}

static void generic_instantiation_record_free(SZrState *state, SZrGenericInstantiationRecord *record) {
    if (state == ZR_NULL || record == ZR_NULL) {
        return;
    }

    if (record->arguments.isValid && record->arguments.head != ZR_NULL) {
        for (TZrSize i = 0; i < record->arguments.length; ++i) {
            SZrGenericInstantiationTypeArgument *argument =
                    (SZrGenericInstantiationTypeArgument *)ZrCore_Array_Get(&record->arguments, i);
            generic_instantiation_argument_free(state, argument);
        }
        ZrCore_Array_Free(state, &record->arguments);
    } else {
        ZrCore_Array_Construct(&record->arguments);
    }

    record->baseToken = 0u;
    record->shareKind = ZR_GENERIC_INSTANTIATION_SHARE_KIND_MONOMORPHIZED_VALUE;
    record->cInstanceId = 0u;
}

void ZrParser_GenericInstantiationTable_Free(SZrState *state, SZrGenericInstantiationTable *table) {
    if (state == ZR_NULL || table == ZR_NULL) {
        return;
    }

    if (table->records.isValid && table->records.head != ZR_NULL) {
        for (TZrSize i = 0; i < table->records.length; ++i) {
            SZrGenericInstantiationRecord *record =
                    (SZrGenericInstantiationRecord *)ZrCore_Array_Get(&table->records, i);
            generic_instantiation_record_free(state, record);
        }
        ZrCore_Array_Free(state, &table->records);
    } else {
        ZrCore_Array_Construct(&table->records);
    }

    table->nextCInstanceId = 1u;
}

TZrSize ZrParser_GenericInstantiationTable_Count(const SZrGenericInstantiationTable *table) {
    if (table == ZR_NULL || !table->records.isValid) {
        return 0u;
    }

    return table->records.length;
}

const SZrGenericInstantiationRecord *ZrParser_GenericInstantiationTable_At(
        const SZrGenericInstantiationTable *table,
        TZrSize index) {
    if (table == ZR_NULL || !table->records.isValid || index >= table->records.length) {
        return ZR_NULL;
    }

    return (const SZrGenericInstantiationRecord *)ZrCore_Array_Get((SZrArray *)&table->records, index);
}

static TZrBool generic_instantiation_record_matches(
        const SZrGenericInstantiationRecord *record,
        TZrUInt32 baseToken,
        const SZrGenericInstantiationTypeArgument *arguments,
        TZrSize argumentCount) {
    if (record == ZR_NULL ||
        arguments == ZR_NULL ||
        record->baseToken != baseToken ||
        record->arguments.length != argumentCount) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < argumentCount; ++i) {
        SZrGenericInstantiationTypeArgument *recordArgument =
                (SZrGenericInstantiationTypeArgument *)ZrCore_Array_Get((SZrArray *)&record->arguments, i);
        if (recordArgument->shape != arguments[i].shape ||
            !ZrParser_InferredType_Equal(&recordArgument->type, &arguments[i].type)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static const SZrGenericInstantiationRecord *generic_instantiation_table_find(
        const SZrGenericInstantiationTable *table,
        TZrUInt32 baseToken,
        const SZrGenericInstantiationTypeArgument *arguments,
        TZrSize argumentCount) {
    if (table == ZR_NULL || !table->records.isValid || arguments == ZR_NULL || argumentCount == 0) {
        return ZR_NULL;
    }

    for (TZrSize i = 0; i < table->records.length; ++i) {
        const SZrGenericInstantiationRecord *record =
                (const SZrGenericInstantiationRecord *)ZrCore_Array_Get((SZrArray *)&table->records, i);
        if (generic_instantiation_record_matches(record, baseToken, arguments, argumentCount)) {
            return record;
        }
    }

    return ZR_NULL;
}

static void generic_instantiation_argument_copy(
        SZrState *state,
        SZrGenericInstantiationTypeArgument *dest,
        const SZrGenericInstantiationTypeArgument *src) {
    ZrParser_InferredType_Copy(state, &dest->type, &src->type);
    dest->shape = src->shape;
}

static void generic_instantiation_record_init_from_arguments(
        SZrState *state,
        SZrGenericInstantiationRecord *record,
        TZrUInt32 baseToken,
        const SZrGenericInstantiationTypeArgument *arguments,
        TZrSize argumentCount,
        TZrUInt32 cInstanceId) {
    record->baseToken = baseToken;
    ZrCore_Array_Init(state, &record->arguments, sizeof(SZrGenericInstantiationTypeArgument), argumentCount);
    for (TZrSize i = 0; i < argumentCount; ++i) {
        SZrGenericInstantiationTypeArgument copiedArgument;
        generic_instantiation_argument_copy(state, &copiedArgument, &arguments[i]);
        ZrCore_Array_Push(state, &record->arguments, &copiedArgument);
    }
    record->shareKind = ZrParser_GenericInstantiation_ComputeShareKind(arguments, argumentCount);
    record->cInstanceId = cInstanceId;
}

TZrBool ZrParser_GenericInstantiationTable_GetOrAddResolved(
        SZrState *state,
        SZrGenericInstantiationTable *table,
        TZrUInt32 baseToken,
        const SZrGenericInstantiationTypeArgument *arguments,
        TZrSize argumentCount,
        const SZrGenericInstantiationRecord **outRecord) {
    const SZrGenericInstantiationRecord *existingRecord;
    SZrGenericInstantiationRecord record;

    if (outRecord != ZR_NULL) {
        *outRecord = ZR_NULL;
    }

    if (state == ZR_NULL ||
        table == ZR_NULL ||
        !table->records.isValid ||
        arguments == ZR_NULL ||
        argumentCount == 0) {
        return ZR_FALSE;
    }

    existingRecord = generic_instantiation_table_find(table, baseToken, arguments, argumentCount);
    if (existingRecord != ZR_NULL) {
        if (outRecord != ZR_NULL) {
            *outRecord = existingRecord;
        }
        return ZR_TRUE;
    }

    generic_instantiation_record_init_from_arguments(
            state,
            &record,
            baseToken,
            arguments,
            argumentCount,
            table->nextCInstanceId++);
    ZrCore_Array_Push(state, &table->records, &record);

    if (outRecord != ZR_NULL) {
        *outRecord = (const SZrGenericInstantiationRecord *)ZrCore_Array_Get(
                &table->records,
                table->records.length - 1u);
    }

    return ZR_TRUE;
}

TZrBool ZrParser_GenericInstantiationTable_GetOrAdd(
        SZrState *state,
        SZrGenericInstantiationTable *table,
        TZrUInt32 baseToken,
        const SZrInferredType *argumentTypes,
        TZrSize argumentCount,
        const SZrGenericInstantiationRecord **outRecord) {
    SZrGenericInstantiationTypeArgument *arguments;
    TZrBool result;

    if (outRecord != ZR_NULL) {
        *outRecord = ZR_NULL;
    }

    if (state == ZR_NULL || argumentTypes == ZR_NULL || argumentCount == 0) {
        return ZR_FALSE;
    }

    arguments = (SZrGenericInstantiationTypeArgument *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrGenericInstantiationTypeArgument) * argumentCount,
            ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (arguments == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < argumentCount; ++i) {
        arguments[i].type = argumentTypes[i];
        arguments[i].shape = ZrParser_GenericInstantiation_InferTypeShape(&argumentTypes[i]);
    }

    result = ZrParser_GenericInstantiationTable_GetOrAddResolved(
            state,
            table,
            baseToken,
            arguments,
            argumentCount,
            outRecord);
    ZrCore_Memory_RawFreeWithType(
            state->global,
            arguments,
            sizeof(SZrGenericInstantiationTypeArgument) * argumentCount,
            ZR_MEMORY_NATIVE_TYPE_ARRAY);

    return result;
}
