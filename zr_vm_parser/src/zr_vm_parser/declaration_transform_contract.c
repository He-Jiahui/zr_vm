#include "zr_vm_parser/declaration_transform_contract.h"

#include <string.h>

static TZrBool declaration_name_is_valid(const TZrChar *name) {
    const unsigned char *cursor = (const unsigned char *)name;

    if (cursor == ZR_NULL ||
        !(cursor[0] == '_' ||
          (cursor[0] >= 'A' && cursor[0] <= 'Z') ||
          (cursor[0] >= 'a' && cursor[0] <= 'z'))) {
        return ZR_FALSE;
    }
    for (cursor++; *cursor != 0U; cursor++) {
        if (!(*cursor == '_' ||
              (*cursor >= 'A' && *cursor <= 'Z') ||
              (*cursor >= 'a' && *cursor <= 'z') ||
              (*cursor >= '0' && *cursor <= '9'))) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool declaration_name_exists(
        const SZrParserDeclarationView *view,
        const SZrParserDeclarationPatch *patch,
        TZrSize additionIndex) {
    const TZrChar *name = patch->additions[additionIndex].name;

    for (TZrSize index = 0; index < view->existingMemberCount; index++) {
        if (view->existingMemberNames != ZR_NULL &&
            view->existingMemberNames[index] != ZR_NULL &&
            strcmp(name, view->existingMemberNames[index]) == 0) {
            return ZR_TRUE;
        }
    }
    for (TZrSize index = 0; index < additionIndex; index++) {
        if (patch->additions[index].name != ZR_NULL &&
            strcmp(name, patch->additions[index].name) == 0) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

EZrParserDeclarationPatchError ZrParser_DeclarationPatch_Validate(
        const SZrParserDeclarationView *view,
        const SZrParserDeclarationPatch *patch) {
    if (view == ZR_NULL || patch == ZR_NULL ||
        view->symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_PARSER_DECLARATION_PATCH_ERROR_ARGUMENT;
    }
    if (patch->targetSymbolId != view->symbolId) {
        return ZR_PARSER_DECLARATION_PATCH_ERROR_TARGET;
    }
    if (patch->expansionRound != 0U) {
        return ZR_PARSER_DECLARATION_PATCH_ERROR_ROUND;
    }
    if (patch->additionCount > ZR_PARSER_DECLARATION_TRANSFORM_MAX_ADDITIONS ||
        patch->interfaceAddCount > ZR_PARSER_DECLARATION_TRANSFORM_MAX_ADDITIONS ||
        patch->attributeAddCount > ZR_PARSER_DECLARATION_TRANSFORM_MAX_ADDITIONS) {
        return ZR_PARSER_DECLARATION_PATCH_ERROR_BUDGET;
    }
    if ((patch->additionCount > 0U && patch->additions == ZR_NULL) ||
        (patch->interfaceAddCount > 0U && patch->interfaceAdds == ZR_NULL) ||
        (patch->attributeAddCount > 0U && patch->attributeAdds == ZR_NULL) ||
        (patch->diagnosticCount > 0U && patch->diagnostics == ZR_NULL)) {
        return ZR_PARSER_DECLARATION_PATCH_ERROR_ARGUMENT;
    }

    for (TZrSize index = 0; index < patch->diagnosticCount; index++) {
        const SZrParserCompileDiagnostic *diagnostic = &patch->diagnostics[index];

        if (diagnostic->message == ZR_NULL || diagnostic->message[0] == '\0') {
            return ZR_PARSER_DECLARATION_PATCH_ERROR_ARGUMENT;
        }
        if (diagnostic->targetSymbolId != view->symbolId) {
            return ZR_PARSER_DECLARATION_PATCH_ERROR_TARGET;
        }
    }

    for (TZrSize index = 0; index < patch->interfaceAddCount; index++) {
        if (patch->interfaceAdds[index] == ZR_SEMANTIC_ID_INVALID) {
            return ZR_PARSER_DECLARATION_PATCH_ERROR_TYPE;
        }
        for (TZrSize previous = 0; previous < index; previous++) {
            if (patch->interfaceAdds[previous] == patch->interfaceAdds[index]) {
                return ZR_PARSER_DECLARATION_PATCH_ERROR_COLLISION;
            }
        }
    }

    for (TZrSize index = 0; index < patch->additionCount; index++) {
        const SZrParserGeneratedDeclaration *addition = &patch->additions[index];

        if (addition->kind != ZR_PARSER_GENERATED_DECLARATION_FIELD) {
            return ZR_PARSER_DECLARATION_PATCH_ERROR_KIND;
        }
        if (!declaration_name_is_valid(addition->name)) {
            return ZR_PARSER_DECLARATION_PATCH_ERROR_NAME;
        }
        if (declaration_name_exists(view, patch, index)) {
            return ZR_PARSER_DECLARATION_PATCH_ERROR_COLLISION;
        }
        if (addition->typeId == ZR_SEMANTIC_ID_INVALID) {
            return ZR_PARSER_DECLARATION_PATCH_ERROR_TYPE;
        }
        if (addition->visibility < ZR_PARSER_GENERATED_VISIBILITY_PRIVATE ||
            addition->visibility > ZR_PARSER_GENERATED_VISIBILITY_PUBLIC) {
            return ZR_PARSER_DECLARATION_PATCH_ERROR_VISIBILITY;
        }
        if (addition->mutability < ZR_PARSER_GENERATED_MUTABILITY_LET ||
            addition->mutability > ZR_PARSER_GENERATED_MUTABILITY_VAR) {
            return ZR_PARSER_DECLARATION_PATCH_ERROR_MUTABILITY;
        }
        for (TZrSize attributeIndex = 0;
             attributeIndex < addition->attributeCount;
             attributeIndex++) {
            if (addition->attributes == ZR_NULL) {
                return ZR_PARSER_DECLARATION_PATCH_ERROR_ARGUMENT;
            }
            if (addition->attributes[attributeIndex].role ==
                ZR_PARSER_ATTRIBUTE_ROLE_DECLARATION_TRANSFORM) {
                return ZR_PARSER_DECLARATION_PATCH_ERROR_RECURSIVE_TRANSFORM;
            }
        }
    }

    return ZR_PARSER_DECLARATION_PATCH_VALID;
}

EZrParserDeclarationPatchError ZrParser_DeclarationView_ValidatePhaseAccess(
        EZrParserCompilePhase currentPhase,
        EZrParserCompilePhase minimumFieldPhase) {
    if (currentPhase < ZR_PARSER_COMPILE_PHASE_BUILD_FACTS ||
        currentPhase > ZR_PARSER_COMPILE_PHASE_LATE_CHECK ||
        minimumFieldPhase < ZR_PARSER_COMPILE_PHASE_BUILD_FACTS ||
        minimumFieldPhase > ZR_PARSER_COMPILE_PHASE_LATE_CHECK) {
        return ZR_PARSER_DECLARATION_PATCH_ERROR_ARGUMENT;
    }
    return currentPhase >= minimumFieldPhase
                   ? ZR_PARSER_DECLARATION_PATCH_VALID
                   : ZR_PARSER_DECLARATION_PATCH_ERROR_PHASE_CYCLE;
}

const TZrChar *ZrParser_DeclarationPatch_ErrorName(
        EZrParserDeclarationPatchError error) {
    static const TZrChar *const names[] = {
            "valid",
            "argument",
            "target",
            "round",
            "budget",
            "kind",
            "name",
            "collision",
            "type",
            "visibility",
            "mutability",
            "recursive_transform",
            "phase_cycle",
    };

    return error >= ZR_PARSER_DECLARATION_PATCH_VALID &&
                   error <= ZR_PARSER_DECLARATION_PATCH_ERROR_PHASE_CYCLE
                   ? names[error]
                   : "unknown";
}
