#include "zr_vm_parser/semantic_value_facts.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_ir.h"

#include <stdlib.h>

typedef enum EZrValueFactsVisit {
    ZR_VALUE_FACTS_UNVISITED = 0,
    ZR_VALUE_FACTS_VISITING,
    ZR_VALUE_FACTS_COMPLETE
} EZrValueFactsVisit;

typedef struct SZrValueFactsFrame {
    TZrSize nodeIndex;
    TZrSize nextEdge;
    TZrSize edgeCount;
} SZrValueFactsFrame;

typedef struct SZrValueFactsGraph {
    const SZrSemanticContext *context;
    EZrValueFactsVisit *visits;
    SZrSemanticValueFacts *facts;
    SZrValueFactsFrame *stack;
} SZrValueFactsGraph;

static TZrBool value_facts_array_shape(const SZrArray *array, TZrSize width) {
    return (TZrBool)(array != ZR_NULL && array->isValid &&
                     array->elementSize == width &&
                     array->length <= array->capacity &&
                     array->capacity <= SIZE_MAX / width &&
                     array->length <= UINT32_MAX &&
                     (array->capacity == 0u || array->head != ZR_NULL));
}

static const SZrCanonicalTypeNode *value_facts_node(
        const SZrValueFactsGraph *graph, TZrSize index) {
    return (const SZrCanonicalTypeNode *)ZrCore_Array_Get(
            (SZrArray *)&graph->context->canonicalTypes, index);
}

static TZrSize value_facts_node_index(const SZrValueFactsGraph *graph, TZrTypeId id) {
    TZrSize low = 0u, high = graph->context->canonicalTypes.length;
    if (id == 0u) return SIZE_MAX;
    while (low < high) {
        TZrSize middle = low + (high - low) / 2u;
        if (value_facts_node(graph, middle)->id < id) low = middle + 1u;
        else high = middle;
    }
    return low < graph->context->canonicalTypes.length &&
                   value_facts_node(graph, low)->id == id ? low : SIZE_MAX;
}

/* Validate local shape before reading any nested array. All child IDs are
 * checked independently by the iterative traversal, even below ref/owner. */
static TZrBool value_facts_node_shape(const SZrCanonicalTypeNode *node,
                                      TZrSize *edgeCount) {
    *edgeCount = 0u;
    switch (node->kind) {
        case ZR_CANONICAL_TYPE_PRIMITIVE:
            return (TZrBool)((TZrUInt32)node->data.primitive.valueType < ZR_VALUE_TYPE_ENUM_MAX);
        case ZR_CANONICAL_TYPE_NOMINAL:
            return (TZrBool)(node->data.nominal.name != ZR_NULL);
        case ZR_CANONICAL_TYPE_GENERIC_PARAMETER:
            return (TZrBool)(node->data.genericParameter.ownerSymbolId != 0u);
        case ZR_CANONICAL_TYPE_GENERIC_INSTANCE:
            if (!value_facts_array_shape(&node->data.genericInstance.arguments,
                                         sizeof(SZrCanonicalGenericArgument)) ||
                node->data.genericInstance.arguments.length == 0u) return ZR_FALSE;
            *edgeCount = node->data.genericInstance.arguments.length + 1u;
            return (TZrBool)(*edgeCount > node->data.genericInstance.arguments.length);
        case ZR_CANONICAL_TYPE_ARRAY:
            *edgeCount = 1u;
            return (TZrBool)(node->data.array.rank != 0u &&
                    (TZrUInt32)node->data.array.storageKind <= ZR_CANONICAL_ARRAY_STORAGE_NATIVE);
        case ZR_CANONICAL_TYPE_TUPLE:
            if (!value_facts_array_shape(&node->data.typeList.elementTypeIds,
                                         sizeof(TZrTypeId))) return ZR_FALSE;
            *edgeCount = node->data.typeList.elementTypeIds.length;
            return ZR_TRUE;
        case ZR_CANONICAL_TYPE_UNION:
            if (!value_facts_array_shape(&node->data.unionType.variantTypeIds,
                                         sizeof(TZrTypeId)) ||
                node->data.unionType.variantTypeIds.length == 0u) return ZR_FALSE;
            *edgeCount = node->data.unionType.variantTypeIds.length + 1u;
            return (TZrBool)(*edgeCount > node->data.unionType.variantTypeIds.length);
        case ZR_CANONICAL_TYPE_REF:
            *edgeCount = 1u;
            return (TZrBool)((TZrUInt32)node->data.refType.access <= ZR_CANONICAL_REF_READONLY);
        case ZR_CANONICAL_TYPE_OWNER:
            *edgeCount = 1u;
            return (TZrBool)((TZrUInt32)node->data.owner.ownerKind <= ZR_CANONICAL_OWNER_ATOMIC_SHARED);
        case ZR_CANONICAL_TYPE_READONLY_VIEW:
        case ZR_CANONICAL_TYPE_NULLABLE:
            *edgeCount = 1u;
            return ZR_TRUE;
        case ZR_CANONICAL_TYPE_FUNCTION:
            if (!value_facts_array_shape(&node->data.function.parameterContracts,
                                         sizeof(SZrCanonicalParameterContract)) ||
                (TZrUInt32)node->data.function.receiverEffect > ZR_CANONICAL_RECEIVER_MUTABLE ||
                (node->data.function.effectFlags & ~(ZR_CANONICAL_CALLABLE_EFFECT_THROWS |
                        ZR_CANONICAL_CALLABLE_EFFECT_ASYNC |
                        ZR_CANONICAL_CALLABLE_EFFECT_GENERATOR)) != 0u) return ZR_FALSE;
            *edgeCount = node->data.function.parameterContracts.length + 1u;
            return (TZrBool)(*edgeCount > node->data.function.parameterContracts.length);
        case ZR_CANONICAL_TYPE_ERROR:
        case ZR_CANONICAL_TYPE_NEVER:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

/* A zero edge denotes a non-type generic argument, never a missing type. */
static TZrBool value_facts_edge(const SZrValueFactsGraph *graph,
                                const SZrCanonicalTypeNode *node,
                                TZrSize edge, TZrTypeId *typeId) {
    const SZrArray *array;
    switch (node->kind) {
        case ZR_CANONICAL_TYPE_GENERIC_INSTANCE:
            if (edge == 0u) *typeId = node->data.genericInstance.definitionTypeId;
            else {
                const SZrCanonicalGenericArgument *argument =
                        (const SZrCanonicalGenericArgument *)ZrCore_Array_Get(
                                (SZrArray *)&node->data.genericInstance.arguments, edge - 1u);
                switch (argument->kind) {
                    case ZR_CANONICAL_GENERIC_ARGUMENT_TYPE:
                        *typeId = argument->data.typeId;
                        break;
                    case ZR_CANONICAL_GENERIC_ARGUMENT_CONST_INT:
                        *typeId = 0u;
                        return ZR_TRUE;
                    case ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER:
                        *typeId = 0u;
                        return (TZrBool)(argument->data.constParameter.ownerSymbolId != 0u);
                    default: return ZR_FALSE;
                }
            }
            break;
        case ZR_CANONICAL_TYPE_ARRAY: *typeId = node->data.array.elementTypeId; break;
        case ZR_CANONICAL_TYPE_TUPLE:
            array = &node->data.typeList.elementTypeIds;
            *typeId = *(const TZrTypeId *)ZrCore_Array_Get((SZrArray *)array, edge);
            break;
        case ZR_CANONICAL_TYPE_UNION:
            array = &node->data.unionType.variantTypeIds;
            *typeId = edge == 0u ? node->data.unionType.definitionTypeId
                    : *(const TZrTypeId *)ZrCore_Array_Get((SZrArray *)array, edge - 1u);
            break;
        case ZR_CANONICAL_TYPE_REF: *typeId = node->data.refType.pointeeTypeId; break;
        case ZR_CANONICAL_TYPE_OWNER: *typeId = node->data.owner.targetTypeId; break;
        case ZR_CANONICAL_TYPE_READONLY_VIEW:
        case ZR_CANONICAL_TYPE_NULLABLE: *typeId = node->data.target.targetTypeId; break;
        case ZR_CANONICAL_TYPE_FUNCTION:
            if (edge == 0u) *typeId = node->data.function.returnTypeId;
            else {
                const SZrCanonicalParameterContract *contract =
                        (const SZrCanonicalParameterContract *)ZrCore_Array_Get(
                                (SZrArray *)&node->data.function.parameterContracts, edge - 1u);
                if (!ZrParser_CanonicalType_ValidateParameterContract(graph->context, contract))
                    return ZR_FALSE;
                *typeId = contract->typeId;
            }
            break;
        default: return ZR_FALSE;
    }
    return (TZrBool)(*typeId != 0u);
}

static void value_facts_derive(SZrValueFactsGraph *graph, TZrSize index) {
    const SZrCanonicalTypeNode *node = value_facts_node(graph, index);
    SZrSemanticValueFacts *facts = &graph->facts[index];
    facts->typeId = node->id;
    switch (node->kind) {
        case ZR_CANONICAL_TYPE_PRIMITIVE:
            if (node->data.primitive.valueType <= ZR_VALUE_TYPE_DOUBLE) {
                facts->ownership = ZR_SEMANTIC_VALUE_OWNERSHIP_VALUE;
                facts->nullability = node->data.primitive.valueType == ZR_VALUE_TYPE_NULL
                        ? ZR_SEMANTIC_VALUE_NULLABILITY_NULLABLE
                        : ZR_SEMANTIC_VALUE_NULLABILITY_NONNULL;
            } else {
                switch (node->data.primitive.valueType) {
                    case ZR_VALUE_TYPE_STRING:
                    case ZR_VALUE_TYPE_BUFFER:
                    case ZR_VALUE_TYPE_ARRAY:
                    case ZR_VALUE_TYPE_FUNCTION:
                    case ZR_VALUE_TYPE_CLOSURE_VALUE:
                    case ZR_VALUE_TYPE_CLOSURE:
                    case ZR_VALUE_TYPE_OBJECT:
                    case ZR_VALUE_TYPE_THREAD:
                        facts->ownership = ZR_SEMANTIC_VALUE_OWNERSHIP_GC;
                        facts->nullability = ZR_SEMANTIC_VALUE_NULLABILITY_NONNULL;
                        break;
                    default: break; /* Raw native payloads do not prove a GC handle. */
                }
            }
            break;
        case ZR_CANONICAL_TYPE_READONLY_VIEW:
        case ZR_CANONICAL_TYPE_NULLABLE:
            *facts = graph->facts[value_facts_node_index(graph, node->data.target.targetTypeId)];
            facts->typeId = node->id;
            if (node->kind == ZR_CANONICAL_TYPE_NULLABLE)
                facts->nullability = ZR_SEMANTIC_VALUE_NULLABILITY_NULLABLE;
            break;
        case ZR_CANONICAL_TYPE_REF:
            facts->ownership = ZR_SEMANTIC_VALUE_OWNERSHIP_BORROWED;
            facts->nullability = ZR_SEMANTIC_VALUE_NULLABILITY_NONNULL;
            break;
        case ZR_CANONICAL_TYPE_OWNER:
            switch (node->data.owner.ownerKind) {
                case ZR_CANONICAL_OWNER_UNIQUE:
                    facts->ownership = ZR_SEMANTIC_VALUE_OWNERSHIP_UNIQUE; break;
                case ZR_CANONICAL_OWNER_SHARED:
                    facts->ownership = ZR_SEMANTIC_VALUE_OWNERSHIP_SHARED; break;
                case ZR_CANONICAL_OWNER_ATOMIC_SHARED:
                    facts->ownership = ZR_SEMANTIC_VALUE_OWNERSHIP_ATOMIC_SHARED; break;
                case ZR_CANONICAL_OWNER_WEAK:
                    facts->ownership = ZR_SEMANTIC_VALUE_OWNERSHIP_WEAK; break;
            }
            facts->nullability = ZR_SEMANTIC_VALUE_NULLABILITY_NONNULL;
            break;
        case ZR_CANONICAL_TYPE_ARRAY:
            if (node->data.array.storageKind == ZR_CANONICAL_ARRAY_STORAGE_MANAGED)
                facts->ownership = ZR_SEMANTIC_VALUE_OWNERSHIP_GC;
            facts->nullability = ZR_SEMANTIC_VALUE_NULLABILITY_NONNULL;
            break;
        case ZR_CANONICAL_TYPE_FUNCTION:
        case ZR_CANONICAL_TYPE_TUPLE:
            facts->nullability = ZR_SEMANTIC_VALUE_NULLABILITY_NONNULL;
            break;
        default:
            /* Nominal/generic/union representation and error/never facts are
             * intentionally unknown. Aggregate scanning is not ownership. */
            break;
    }
}

static TZrBool value_facts_visit(SZrValueFactsGraph *graph, TZrSize root) {
    TZrSize depth = 1u;
    graph->stack[0].nodeIndex = root;
    graph->stack[0].nextEdge = 0u;
    while (depth != 0u) {
        SZrValueFactsFrame *frame = &graph->stack[depth - 1u];
        const SZrCanonicalTypeNode *node = value_facts_node(graph, frame->nodeIndex);
        if (graph->visits[frame->nodeIndex] == ZR_VALUE_FACTS_UNVISITED) {
            if (!value_facts_node_shape(node, &frame->edgeCount)) return ZR_FALSE;
            graph->visits[frame->nodeIndex] = ZR_VALUE_FACTS_VISITING;
        }
        if (frame->nextEdge < frame->edgeCount) {
            TZrTypeId child;
            TZrSize childIndex;
            if (!value_facts_edge(graph, node, frame->nextEdge++, &child)) return ZR_FALSE;
            if (child == 0u) continue;
            childIndex = value_facts_node_index(graph, child);
            if (childIndex == SIZE_MAX || graph->visits[childIndex] == ZR_VALUE_FACTS_VISITING)
                return ZR_FALSE;
            if (graph->visits[childIndex] == ZR_VALUE_FACTS_COMPLETE) continue;
            if (depth >= graph->context->canonicalTypes.length) return ZR_FALSE;
            graph->stack[depth].nodeIndex = childIndex;
            graph->stack[depth++].nextEdge = 0u;
        } else {
            value_facts_derive(graph, frame->nodeIndex);
            graph->visits[frame->nodeIndex] = ZR_VALUE_FACTS_COMPLETE;
            depth--;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticIr_ResolveValueFacts(
        SZrSemanticIrFunction *function, const SZrSemanticContext *context) {
    SZrValueFactsGraph graph = {0};
    SZrSemanticValueFacts *staged = ZR_NULL;
    TZrSize index, count;
    TZrTypeId previousId = 0u;
    TZrBool ok = ZR_FALSE;
    if (function == ZR_NULL || context == ZR_NULL ||
        !value_facts_array_shape(&function->values, sizeof(SZrSemanticIrValue)) ||
        !value_facts_array_shape(&context->canonicalTypes, sizeof(SZrCanonicalTypeNode)))
        return ZR_FALSE;
    graph.context = context;
    count = context->canonicalTypes.length;
    for (index = 0u; index < count; index++) {
        const SZrCanonicalTypeNode *node = value_facts_node(&graph, index);
        if (node->id <= previousId) return ZR_FALSE;
        previousId = node->id;
    }
    if (function->values.length == 0u) return ZR_TRUE;
    if (count > SIZE_MAX / sizeof(*graph.visits) ||
        count > SIZE_MAX / sizeof(*graph.facts) ||
        count > SIZE_MAX / sizeof(*graph.stack) ||
        function->values.length > SIZE_MAX / sizeof(*staged)) return ZR_FALSE;
    if (count != 0u) {
        graph.visits = (EZrValueFactsVisit *)calloc(count, sizeof(*graph.visits));
        graph.facts = (SZrSemanticValueFacts *)calloc(count, sizeof(*graph.facts));
        graph.stack = (SZrValueFactsFrame *)calloc(count, sizeof(*graph.stack));
        if (graph.visits == ZR_NULL || graph.facts == ZR_NULL || graph.stack == ZR_NULL) goto done;
    }
    staged = (SZrSemanticValueFacts *)calloc(function->values.length, sizeof(*staged));
    if (staged == ZR_NULL) goto done;
    for (index = 0u; index < function->values.length; index++) {
        const SZrSemanticIrValue *value = (const SZrSemanticIrValue *)ZrCore_Array_Get(
                &function->values, index);
        TZrSize typeIndex;
        if (value->id != index + 1u) goto done;
        if (value->typeId == 0u) continue;
        typeIndex = value_facts_node_index(&graph, value->typeId);
        if (typeIndex == SIZE_MAX ||
            (graph.visits[typeIndex] != ZR_VALUE_FACTS_COMPLETE &&
             !value_facts_visit(&graph, typeIndex))) goto done;
        staged[index] = graph.facts[typeIndex];
    }
    for (index = 0u; index < function->values.length; index++) {
        SZrSemanticIrValue *value = (SZrSemanticIrValue *)ZrCore_Array_Get(&function->values, index);
        value->facts = staged[index];
    }
    ok = ZR_TRUE;
done:
    free(staged);
    free(graph.stack);
    free(graph.facts);
    free(graph.visits);
    return ok;
}
