#include "type_inference_internal.h"

#include <string.h>

#include "compiler_internal.h"
#include "compiler_type_declaration.h"
#include "compiler_type_value_alias.h"
#include "zr_vm_core/string.h"

TZrBool type_name_is_explicitly_available_with_context_inference(
        SZrCompilerState *cs, SZrString *typeName, SZrSyntaxContext syntaxContext) {
    const TZrChar *typeNameText;

    if (cs == ZR_NULL || typeName == ZR_NULL ||
        !ZrParser_SyntaxContext_IsValid(syntaxContext)) {
        return ZR_FALSE;
    }

    typeNameText = ZrCore_String_GetNativeString(typeName);
    if (typeNameText != ZR_NULL && strcmp(typeNameText, "zr") == 0) {
        if (find_compiler_type_prototype_inference(cs, typeName) != ZR_NULL ||
            (ensure_import_module_compile_info(cs, typeName) &&
             find_compiler_type_prototype_inference(cs, typeName) != ZR_NULL)) {
            return ZR_TRUE;
        }
    }

    if (typeNameText != ZR_NULL && strchr(typeNameText, '.') != ZR_NULL &&
        find_compiler_type_prototype_inference(cs, typeName) != ZR_NULL) {
        return ZR_TRUE;
    }

    if (compiler_find_type_value_alias(cs, typeName, syntaxContext) != ZR_NULL ||
        (cs->typeEnv != ZR_NULL && ZrParser_TypeEnvironment_LookupTypeWithContext(
                cs->typeEnv, typeName, syntaxContext)) ||
        type_inference_current_generic_context_contains_type_name(cs, typeName)) {
        return ZR_TRUE;
    }

    return find_type_declaration_with_context(cs, typeName, syntaxContext) != ZR_NULL;
}

TZrBool type_name_is_explicitly_available_in_context_inference(
        SZrCompilerState *cs, SZrString *typeName) {
    return type_name_is_explicitly_available_with_context_inference(
            cs, typeName, ZrParser_SyntaxContext_Root());
}
