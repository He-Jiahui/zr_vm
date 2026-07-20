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
        contract->kind != ZR_LSP_EXTERNAL_CALLABLE_FUNCTION ||
        buffer == ZR_NULL || bufferSize == 0U ||
        contract->parameters == ZR_NULL || index >= contract->parameterCount) {
        return ZR_FALSE;
    }
    parameter = &contract->parameters[index];
    if (!external_callable_text_is_available(parameter->typeName)) {
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
        contract->kind != ZR_LSP_EXTERNAL_CALLABLE_FUNCTION ||
        buffer == ZR_NULL || bufferSize == 0U ||
        !external_callable_text_is_available(contract->name) ||
        !external_callable_text_is_available(contract->returnTypeName)) {
        return ZR_FALSE;
    }

    if (!external_callable_append(
                buffer, bufferSize, &offset, "%s", contract->name)) {
        return ZR_FALSE;
    }

    if (contract->genericParameterCount > 0U) {
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
    return external_callable_append(
            buffer,
            bufferSize,
            &offset,
            "): %s",
            contract->returnTypeName);
}
