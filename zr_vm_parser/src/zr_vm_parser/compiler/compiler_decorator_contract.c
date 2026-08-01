#include "compiler_decorator_contract.h"

#include "compiler_attribute_binding.h"
#include "compiler_internal.h"

static const TZrChar *const kBuiltinFfiWrapperLeafNames[] = {
        "lowering",
        "viewType",
        "underlying",
        "ownerMode",
        "releaseHook",
};

const TZrChar *ZrParser_DecoratorContract_BuiltinFfiWrapperLeafName(
        SZrAstNode *decoratorNode,
        TZrBool *outHasCall) {
    if (outHasCall != ZR_NULL) {
        *outHasCall = ZR_FALSE;
    }

    for (TZrSize index = 0;
         index < ZR_ARRAY_COUNT(kBuiltinFfiWrapperLeafNames);
         index++) {
        const TZrChar *leafName = kBuiltinFfiWrapperLeafNames[index];

        if (extern_compiler_match_decorator_path(
                    decoratorNode, leafName, ZR_TRUE, ZR_NULL)) {
            if (outHasCall != ZR_NULL) {
                *outHasCall = ZR_TRUE;
            }
            return leafName;
        }
        if (extern_compiler_match_decorator_path(
                    decoratorNode, leafName, ZR_FALSE, ZR_NULL)) {
            return leafName;
        }
    }

    return ZR_NULL;
}

TZrBool ZrParser_DecoratorContract_IsBuiltinFfiWrapper(
        SZrAstNode *decoratorNode) {
    return ZrParser_DecoratorContract_BuiltinFfiWrapperLeafName(
                   decoratorNode, ZR_NULL) != ZR_NULL
           ? ZR_TRUE
           : ZR_FALSE;
}

TZrBool ZrParser_DecoratorContract_ValidateNoRuntimeDecorators(
        SZrCompilerState *cs,
        SZrAstNodeArray *decorators,
        TZrBool allowBuiltinFfiWrapper) {
    if (cs == ZR_NULL) {
        return ZR_FALSE;
    }
    if (decorators == ZR_NULL || decorators->count == 0U) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];

        if (decoratorNode == ZR_NULL ||
            ZrParser_Metadata_IsRegisteredAttribute(cs, decoratorNode) ||
            ZrParser_Compiler_IsCompileTimeDecorator(cs, decoratorNode) ||
            (allowBuiltinFfiWrapper &&
             ZrParser_DecoratorContract_IsBuiltinFfiWrapper(decoratorNode))) {
            if (cs->hasError) {
                return ZR_FALSE;
            }
            continue;
        }

        ZrParser_Compiler_Error(
                cs,
                "decorator.runtime_removed: use retained attribute data or an explicit runtime call",
                decoratorNode->location);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}
