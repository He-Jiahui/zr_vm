#include "compiler_child_function_relocation.h"

static void compiler_relocate_child_constants_in_owned_graph(
        SZrFunction *function,
        SZrFunction *const *originalChildren,
        SZrFunction *copiedChildren,
        TZrUInt32 childCount) {
    if (function == ZR_NULL || function->childFunctionGraphIsBorrowed) {
        return;
    }

    if (function->constantValueList != ZR_NULL) {
        for (TZrUInt32 constantIndex = 0U;
             constantIndex < function->constantValueLength;
             constantIndex++) {
            SZrTypeValue *constant = &function->constantValueList[constantIndex];
            SZrRawObject *rawObject;

            if ((constant->type != ZR_VALUE_TYPE_FUNCTION &&
                 constant->type != ZR_VALUE_TYPE_CLOSURE) ||
                !constant->isGarbageCollectable || constant->isNative ||
                constant->value.object == ZR_NULL) {
                continue;
            }
            rawObject = constant->value.object;
            if (rawObject->type != ZR_RAW_OBJECT_TYPE_FUNCTION || rawObject->isNative) {
                continue;
            }
            for (TZrUInt32 childIndex = 0U; childIndex < childCount; childIndex++) {
                if (originalChildren[childIndex] != ZR_NULL &&
                    rawObject == &originalChildren[childIndex]->super) {
                    constant->value.object = &copiedChildren[childIndex].super;
                    break;
                }
            }
        }
    }

    if (function->childFunctionList != ZR_NULL) {
        for (TZrUInt32 childIndex = 0U;
             childIndex < function->childFunctionLength;
             childIndex++) {
            compiler_relocate_child_constants_in_owned_graph(
                    &function->childFunctionList[childIndex],
                    originalChildren,
                    copiedChildren,
                    childCount);
        }
    }
}

void compiler_relocate_copied_child_constants(
        SZrFunction *parent,
        SZrFunction *const *originalChildren) {
    if (parent == ZR_NULL || originalChildren == ZR_NULL ||
        parent->childFunctionList == ZR_NULL) {
        return;
    }
    compiler_relocate_child_constants_in_owned_graph(
            parent, originalChildren, parent->childFunctionList, parent->childFunctionLength);
}
