#include "zr_vm_core/call_binding.h"

#include <stdlib.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"

static TZrBool graph_append(SZrFunction ***functions, TZrSize *count,
                            TZrSize *capacity, SZrFunction *function) {
    SZrFunction **resized;
    TZrSize nextCapacity;
    if (function == ZR_NULL) return ZR_TRUE;
    for (TZrSize index = 0u; index < *count; ++index) {
        if ((*functions)[index] == function) return ZR_TRUE;
    }
    if (*count == *capacity) {
        nextCapacity = *capacity == 0u ? 16u : *capacity * 2u;
        if (nextCapacity < *capacity || nextCapacity > SIZE_MAX / sizeof(**functions)) return ZR_FALSE;
        resized = realloc(*functions, nextCapacity * sizeof(**functions));
        if (resized == ZR_NULL) return ZR_FALSE;
        *functions = resized;
        *capacity = nextCapacity;
    }
    (*functions)[(*count)++] = function;
    return ZR_TRUE;
}

TZrBool ZrCore_CallBinding_VisitFunctions(SZrFunction *root,
        FZrCallBindingFunctionVisitor visitor, void *context) {
    SZrFunction **functions = ZR_NULL;
    TZrSize count = 0u, capacity = 0u;
    TZrBool result = ZR_FALSE;
    if (root == ZR_NULL || visitor == ZR_NULL ||
        !graph_append(&functions, &count, &capacity, root)) goto cleanup;
    /* Materialize the graph first. Constant method bodies are not necessarily
     * inline children, and shared constants can point back to an earlier body. */
    for (TZrSize index = 0u; index < count; ++index) {
        SZrFunction *function = functions[index];
        if ((function->childFunctionLength != 0u && function->childFunctionList == ZR_NULL) ||
            (function->constantValueLength != 0u && function->constantValueList == ZR_NULL)) goto cleanup;
        for (TZrUInt32 child = 0u; child < function->childFunctionLength; ++child) {
            if (!graph_append(&functions, &count, &capacity, &function->childFunctionList[child])) goto cleanup;
        }
        for (TZrUInt32 constant = 0u; constant < function->constantValueLength; ++constant) {
            const SZrTypeValue *value = &function->constantValueList[constant];
            SZrFunction *target = ZR_NULL;
            if (value->isNative ||
                (value->type != ZR_VALUE_TYPE_FUNCTION && value->type != ZR_VALUE_TYPE_CLOSURE) ||
                value->value.object == ZR_NULL || value->value.object->isNative) continue;
            if (value->value.object->type == ZR_RAW_OBJECT_TYPE_FUNCTION)
                target = (SZrFunction *)value->value.object;
            else if (value->value.object->type == ZR_RAW_OBJECT_TYPE_CLOSURE)
                target = ((SZrClosure *)value->value.object)->function;
            if (!graph_append(&functions, &count, &capacity, target)) goto cleanup;
        }
    }
    for (TZrSize index = 0u; index < count; ++index) {
        if (!visitor(functions[index], context)) goto cleanup;
    }
    result = ZR_TRUE;
cleanup:
    free(functions);
    return result;
}
