#include "semantic/lsp_external_callable_contract.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static TZrBool external_callable_text_is_available(const TZrChar *text) {
    return text != ZR_NULL && text[0] != '\0' &&
           strcmp(text, "cannot infer exact type") != 0 &&
           strcmp(text, "unknown") != 0;
}

static TZrBool external_callable_append(TZrChar *buffer,
                                        TZrSize bufferSize,
                                        TZrSize *offset,
                                        const TZrChar *format,
                                        ...) {
    va_list args;
    TZrInt32 written;

    if (buffer == ZR_NULL || bufferSize == 0U || offset == ZR_NULL ||
        *offset >= bufferSize || format == ZR_NULL) {
        return ZR_FALSE;
    }

    va_start(args, format);
    written = vsnprintf(buffer + *offset, bufferSize - *offset, format, args);
    va_end(args);
    if (written < 0 || (TZrSize)written >= bufferSize - *offset) {
        buffer[bufferSize - 1U] = '\0';
        return ZR_FALSE;
    }
    *offset += (TZrSize)written;
    return ZR_TRUE;
}

static const TZrChar *external_callable_passing_prefix(
        const SZrCanonicalParameterContract *parameter) {
    if (parameter == ZR_NULL) {
        return ZR_NULL;
    }
    switch (parameter->passingForm) {
        case ZR_CANONICAL_PASSING_IN: return "in ";
        case ZR_CANONICAL_PASSING_REF:
            return parameter->escapeUpperBound == ZR_CANONICAL_ESCAPE_FUNCTION
                           ? "scoped ref "
                           : "ref ";
        case ZR_CANONICAL_PASSING_REF_READONLY:
            return parameter->escapeUpperBound == ZR_CANONICAL_ESCAPE_FUNCTION
                           ? "scoped ref readonly "
                           : "ref readonly ";
        case ZR_CANONICAL_PASSING_OUT: return "out ";
        case ZR_CANONICAL_PASSING_VALUE: return "";
        default: return ZR_NULL;
    }
}

static TZrTypeId external_callable_parameter_value_type_id(
        const SZrLspExternalCallableContract *contract,
        const SZrCanonicalParameterContract *parameter) {
    const SZrCanonicalTypeNode *type;

    if (contract == ZR_NULL || contract->canonicalContext == ZR_NULL ||
        parameter == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    if (parameter->passingForm == ZR_CANONICAL_PASSING_VALUE) {
        return parameter->typeId;
    }
    type = ZrParser_CanonicalType_Find(
            contract->canonicalContext, parameter->typeId);
    return type != ZR_NULL && type->kind == ZR_CANONICAL_TYPE_REF
                   ? type->data.refType.pointeeTypeId
                   : ZR_SEMANTIC_ID_INVALID;
}

TZrBool ZrLanguageServer_LspExternalCallableContract_FromResolvedMember(
        const SZrLspResolvedMetadataMember *member,
        SZrLspExternalCallableContract *contract) {
    if (contract != ZR_NULL) {
        memset(contract, 0, sizeof(*contract));
    }
    if (member == ZR_NULL || contract == ZR_NULL) {
        return ZR_FALSE;
    }

    if (member->memberKind == ZR_LSP_METADATA_MEMBER_FUNCTION &&
        member->functionDescriptor != ZR_NULL) {
        const ZrLibFunctionDescriptor *descriptor = member->functionDescriptor;
        contract->kind = ZR_LSP_EXTERNAL_CALLABLE_FUNCTION;
        contract->name = descriptor->name;
        contract->returnTypeName = descriptor->returnTypeName;
        contract->documentation = descriptor->documentation;
        contract->parameters = descriptor->parameters;
        contract->parameterCount = descriptor->parameterCount;
        contract->genericParameters = descriptor->genericParameters;
        contract->genericParameterCount = descriptor->genericParameterCount;
    } else {
        return ZR_FALSE;
    }

    return external_callable_text_is_available(contract->name) &&
           external_callable_text_is_available(contract->returnTypeName) &&
           (contract->parameterCount == 0U || contract->parameters != ZR_NULL) &&
           (contract->genericParameterCount == 0U ||
            contract->genericParameters != ZR_NULL);
}

TZrBool ZrLanguageServer_LspExternalCallableContract_FromResolvedMethod(
        const SZrLspResolvedMetadataMember *member,
        const SZrSemanticContext *canonicalContext,
        TZrTypeId callableTypeId,
        SZrLspExternalCallableContract *contract) {
    const ZrLibMethodDescriptor *descriptor;
    const SZrCanonicalTypeNode *functionType;

    if (contract != ZR_NULL) {
        memset(contract, 0, sizeof(*contract));
    }
    if (member == ZR_NULL || canonicalContext == ZR_NULL ||
        callableTypeId == ZR_SEMANTIC_ID_INVALID || contract == ZR_NULL ||
        member->memberKind != ZR_LSP_METADATA_MEMBER_METHOD ||
        member->methodDescriptor == ZR_NULL) {
        return ZR_FALSE;
    }

    descriptor = member->methodDescriptor;
    functionType = ZrParser_CanonicalType_Find(
            canonicalContext, callableTypeId);
    if (functionType == ZR_NULL ||
        functionType->kind != ZR_CANONICAL_TYPE_FUNCTION ||
        functionType->data.function.receiverEffect ==
                ZR_CANONICAL_RECEIVER_NONE ||
        functionType->data.function.effectFlags !=
                ZR_CANONICAL_CALLABLE_EFFECT_NONE ||
        descriptor->isStatic ||
        descriptor->genericParameterCount != 0U ||
        !external_callable_text_is_available(descriptor->returnTypeName) ||
        functionType->data.function.parameterContracts.length !=
                descriptor->parameterCount ||
        (descriptor->parameterCount > 0U && descriptor->parameters == ZR_NULL)) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < descriptor->parameterCount; index++) {
        if (!external_callable_text_is_available(
                    descriptor->parameters[index].typeName)) {
            return ZR_FALSE;
        }
    }

    contract->kind = ZR_LSP_EXTERNAL_CALLABLE_METHOD;
    contract->name = descriptor->name;
    contract->documentation = descriptor->documentation;
    contract->parameters = descriptor->parameters;
    contract->parameterCount = descriptor->parameterCount;
    contract->canonicalContext = canonicalContext;
    contract->canonicalFunctionType = functionType;
    return external_callable_text_is_available(contract->name);
}

TZrBool ZrLanguageServer_LspExternalCallableContract_FormatParameter(
        const SZrLspExternalCallableContract *contract,
        TZrSize index,
        TZrChar *buffer,
        TZrSize bufferSize) {
    const ZrLibParameterDescriptor *parameter;
    TZrInt32 written;

    if (buffer != ZR_NULL && bufferSize > 0U) {
        buffer[0] = '\0';
    }
    if (contract == ZR_NULL ||
        buffer == ZR_NULL || bufferSize == 0U ||
        contract->parameters == ZR_NULL || index >= contract->parameterCount) {
        return ZR_FALSE;
    }
    parameter = &contract->parameters[index];
    if (contract->kind == ZR_LSP_EXTERNAL_CALLABLE_METHOD) {
        const SZrCanonicalParameterContract *canonicalParameter;
        const TZrChar *passingPrefix;
        TZrTypeId valueTypeId;
        TZrChar typeLabel[ZR_LSP_TEXT_BUFFER_LENGTH];

        if (contract->canonicalContext == ZR_NULL ||
            contract->canonicalFunctionType == ZR_NULL ||
            contract->canonicalFunctionType->kind !=
                    ZR_CANONICAL_TYPE_FUNCTION) {
            return ZR_FALSE;
        }
        canonicalParameter =
                (const SZrCanonicalParameterContract *)ZrCore_Array_Get(
                        (SZrArray *)&contract->canonicalFunctionType->data.function
                                .parameterContracts,
                        index);
        passingPrefix = external_callable_passing_prefix(canonicalParameter);
        valueTypeId = external_callable_parameter_value_type_id(
                contract, canonicalParameter);
        if (passingPrefix == ZR_NULL ||
            valueTypeId == ZR_SEMANTIC_ID_INVALID ||
            !ZrParser_CanonicalType_Format(
                    contract->canonicalContext,
                    valueTypeId,
                    typeLabel,
                    sizeof(typeLabel))) {
            return ZR_FALSE;
        }
        written = parameter->name != ZR_NULL && parameter->name[0] != '\0'
                          ? snprintf(buffer,
                                     bufferSize,
                                     "%s: %s%s",
                                     parameter->name,
                                     passingPrefix,
                                     typeLabel)
                          : snprintf(buffer,
                                     bufferSize,
                                     "%s%s",
                                     passingPrefix,
                                     typeLabel);
        return written >= 0 && (TZrSize)written < bufferSize;
    }

    if (contract->kind != ZR_LSP_EXTERNAL_CALLABLE_FUNCTION ||
        !external_callable_text_is_available(parameter->typeName)) {
        return ZR_FALSE;
    }

    written = parameter->name != ZR_NULL && parameter->name[0] != '\0'
                      ? snprintf(buffer,
                                 bufferSize,
                                 "%s: %s",
                                 parameter->name,
                                 parameter->typeName)
                      : snprintf(buffer, bufferSize, "%s", parameter->typeName);
    return written >= 0 && (TZrSize)written < bufferSize;
}

TZrBool ZrLanguageServer_LspExternalCallableContract_Format(
        const SZrLspExternalCallableContract *contract,
        TZrChar *buffer,
        TZrSize bufferSize) {
    TZrSize offset = 0U;

    if (buffer != ZR_NULL && bufferSize > 0U) {
        buffer[0] = '\0';
    }
    if (contract == ZR_NULL ||
        buffer == ZR_NULL || bufferSize == 0U ||
        !external_callable_text_is_available(contract->name)) {
        return ZR_FALSE;
    }

    if (contract->kind == ZR_LSP_EXTERNAL_CALLABLE_METHOD) {
        const TZrChar *prefix;

        if (contract->canonicalContext == ZR_NULL ||
            contract->canonicalFunctionType == ZR_NULL ||
            contract->canonicalFunctionType->kind !=
                    ZR_CANONICAL_TYPE_FUNCTION) {
            return ZR_FALSE;
        }
        prefix = contract->canonicalFunctionType->data.function.receiverEffect ==
                         ZR_CANONICAL_RECEIVER_READONLY
                         ? "const fn "
                         : (contract->canonicalFunctionType->data.function
                                            .receiverEffect ==
                                    ZR_CANONICAL_RECEIVER_MUTABLE
                                    ? "fn "
                                    : ZR_NULL);
        if (prefix == ZR_NULL ||
            !external_callable_append(
                    buffer,
                    bufferSize,
                    &offset,
                    "%s%s",
                    prefix,
                    contract->name)) {
            return ZR_FALSE;
        }
    } else if (contract->kind != ZR_LSP_EXTERNAL_CALLABLE_FUNCTION ||
               !external_callable_text_is_available(
                       contract->returnTypeName) ||
               !external_callable_append(
                       buffer,
                       bufferSize,
                       &offset,
                       "%s",
                       contract->name)) {
        return ZR_FALSE;
    }

    if (contract->kind == ZR_LSP_EXTERNAL_CALLABLE_FUNCTION &&
        contract->genericParameterCount > 0U) {
        if (!external_callable_append(buffer, bufferSize, &offset, "<")) {
            return ZR_FALSE;
        }
        for (TZrSize index = 0U;
             index < contract->genericParameterCount;
             index++) {
            const ZrLibGenericParameterDescriptor *parameter =
                    &contract->genericParameters[index];
            if (!external_callable_text_is_available(parameter->name) ||
                parameter->constraintTypeCount > 0U ||
                !external_callable_append(
                        buffer,
                        bufferSize,
                        &offset,
                        "%s%s",
                        index > 0U ? ", " : "",
                        parameter->name)) {
                return ZR_FALSE;
            }
        }
        if (!external_callable_append(buffer, bufferSize, &offset, ">")) {
            return ZR_FALSE;
        }
    }

    if (!external_callable_append(buffer, bufferSize, &offset, "(")) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < contract->parameterCount; index++) {
        TZrChar parameterLabel[ZR_LSP_TEXT_BUFFER_LENGTH];
        if (!ZrLanguageServer_LspExternalCallableContract_FormatParameter(
                    contract,
                    index,
                    parameterLabel,
                    sizeof(parameterLabel)) ||
            !external_callable_append(
                    buffer,
                    bufferSize,
                    &offset,
                    "%s%s",
                    index > 0U ? ", " : "",
                    parameterLabel)) {
            return ZR_FALSE;
        }
    }
    if (contract->kind == ZR_LSP_EXTERNAL_CALLABLE_METHOD) {
        TZrChar returnTypeLabel[ZR_LSP_TEXT_BUFFER_LENGTH];
        if (!ZrParser_CanonicalType_Format(
                    contract->canonicalContext,
                    contract->canonicalFunctionType->data.function.returnTypeId,
                    returnTypeLabel,
                    sizeof(returnTypeLabel))) {
            return ZR_FALSE;
        }
        return external_callable_append(
                buffer, bufferSize, &offset, "): %s", returnTypeLabel);
    }
    return external_callable_append(
            buffer, bufferSize, &offset, "): %s", contract->returnTypeName);
}
