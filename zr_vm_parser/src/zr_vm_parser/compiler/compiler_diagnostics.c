//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"

#include <stdarg.h>

static void compiler_report_error(SZrCompilerState *cs,
                                  const TZrChar *msg,
                                  SZrFileRange location,
                                  TZrBool clearStructuredError);

static void compiler_store_error_message(SZrCompilerState *cs, const TZrChar *message) {
    TZrSize requiredSize = 0;
    TZrChar *newBuffer = ZR_NULL;

    if (cs == ZR_NULL) {
        return;
    }

    if (message == ZR_NULL) {
        cs->errorMessage = ZR_NULL;
        return;
    }

    requiredSize = strlen(message) + 1;
    if (requiredSize > cs->errorMessageStorageCapacity) {
        newBuffer = (TZrChar *)ZrCore_Memory_Allocate(cs->state->global,
                                                      cs->errorMessageStorage,
                                                      cs->errorMessageStorageCapacity,
                                                      requiredSize,
                                                      ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
        if (newBuffer == ZR_NULL) {
            cs->errorMessage = "Failed to allocate compiler error message";
            return;
        }

        cs->errorMessageStorage = newBuffer;
        cs->errorMessageStorageCapacity = requiredSize;
    }

    memcpy(cs->errorMessageStorage, message, requiredSize);
    cs->errorMessage = cs->errorMessageStorage;
}

void ZrParser_Compiler_ClearStructuredError(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return;
    }

    if (cs->hasStructuredError) {
        ZrParser_StructuredDiagnostic_Free(cs->state, &cs->structuredError);
        ZrParser_StructuredDiagnostic_Init(&cs->structuredError);
        cs->hasStructuredError = ZR_FALSE;
    }
}

void ZrParser_Compiler_StructuredError(SZrCompilerState *cs, const SZrStructuredDiagnostic *diagnostic) {
    const TZrChar *messageText;

    if (cs == ZR_NULL || diagnostic == ZR_NULL || diagnostic->message == ZR_NULL) {
        return;
    }

    ZrParser_Compiler_ClearStructuredError(cs);
    cs->structuredError = *diagnostic;
    cs->hasStructuredError = ZR_TRUE;

    messageText = ZrCore_String_GetNativeString(diagnostic->message);
    if (messageText == ZR_NULL) {
        messageText = "Compiler diagnostic";
    }
    compiler_report_error(cs, messageText, diagnostic->location, ZR_FALSE);
    cs->hasStructuredError = ZR_TRUE;
}

static TZrBool compiler_duplicate_type_declaration_identity(
        SZrAstNode *declaration,
        SZrString **outName,
        SZrFileRange *outNameRange) {
    if (outName != ZR_NULL) {
        *outName = ZR_NULL;
    }
    if (outNameRange != ZR_NULL) {
        memset(outNameRange, 0, sizeof(*outNameRange));
    }
    if (declaration == ZR_NULL || outName == ZR_NULL || outNameRange == ZR_NULL) {
        return ZR_FALSE;
    }

    *outNameRange = declaration->location;
    switch (declaration->type) {
        case ZR_AST_CLASS_DECLARATION:
            if (declaration->data.classDeclaration.name == ZR_NULL) {
                return ZR_FALSE;
            }
            *outName = declaration->data.classDeclaration.name->name;
            *outNameRange = declaration->data.classDeclaration.nameLocation;
            break;
        case ZR_AST_STRUCT_DECLARATION:
            if (declaration->data.structDeclaration.name == ZR_NULL) {
                return ZR_FALSE;
            }
            *outName = declaration->data.structDeclaration.name->name;
            break;
        case ZR_AST_INTERFACE_DECLARATION:
            if (declaration->data.interfaceDeclaration.name == ZR_NULL) {
                return ZR_FALSE;
            }
            *outName = declaration->data.interfaceDeclaration.name->name;
            break;
        case ZR_AST_ENUM_DECLARATION:
            if (declaration->data.enumDeclaration.name == ZR_NULL) {
                return ZR_FALSE;
            }
            *outName = declaration->data.enumDeclaration.name->name;
            break;
        case ZR_AST_UNION_DECLARATION:
            if (declaration->data.unionDeclaration.name == ZR_NULL) {
                return ZR_FALSE;
            }
            *outName = declaration->data.unionDeclaration.name->name;
            break;
        default:
            return ZR_FALSE;
    }
    return *outName != ZR_NULL;
}

static TZrBool compiler_report_duplicate_type_binding(
        SZrCompilerState *cs,
        SZrString *name,
        SZrFileRange location,
        const SZrFileRange *previousLocation) {
    SZrStructuredDiagnostic diagnostic;
    const TZrChar *nameText;
    TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];

    if (cs == ZR_NULL || cs->state == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    nameText = ZrCore_String_GetNativeString(name);
    if (nameText != ZR_NULL) {
        snprintf(message,
                 sizeof(message),
                 "Type name '%s' is already declared in this context",
                 nameText);
    } else {
        snprintf(message, sizeof(message), "Type name is already declared in this context");
    }

    ZrParser_StructuredDiagnostic_Init(&diagnostic);
    if (!ZrParser_DiagnosticBuilder_Build(
                cs->state,
                &diagnostic,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "duplicate_type",
                message,
                "Another type binding with the same name is already visible in this context.",
                "Rename this type or remove the duplicate declaration.") ||
        !ZrParser_StructuredDiagnostic_SetNoFixReason(
                &diagnostic,
                ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION)) {
        ZrParser_StructuredDiagnostic_Free(cs->state, &diagnostic);
        ZrParser_Compiler_Error(cs, message, location);
        return ZR_FALSE;
    }
    if (previousLocation != ZR_NULL &&
        !ZrParser_StructuredDiagnostic_AddRelatedInformation(
                cs->state,
                &diagnostic,
                *previousLocation,
                "Type was first declared here")) {
        ZrParser_StructuredDiagnostic_Free(cs->state, &diagnostic);
        ZrParser_Compiler_Error(cs, message, location);
        return ZR_FALSE;
    }

    ZrParser_Compiler_StructuredError(cs, &diagnostic);
    return ZR_TRUE;
}

TZrBool ZrParser_Compiler_ReportDuplicateTypeDeclaration(
        SZrCompilerState *cs,
        SZrAstNode *declaration,
        SZrAstNode *previousDeclaration) {
    SZrString *name;
    SZrString *previousName;
    SZrFileRange location;
    SZrFileRange previousLocation;
    const SZrFileRange *previousLocationPtr = ZR_NULL;

    if (cs == ZR_NULL ||
        !compiler_duplicate_type_declaration_identity(
                declaration, &name, &location)) {
        return ZR_FALSE;
    }
    if (previousDeclaration != ZR_NULL &&
        compiler_duplicate_type_declaration_identity(
                previousDeclaration, &previousName, &previousLocation) &&
        previousName != ZR_NULL && ZrCore_String_Equal(previousName, name)) {
        previousLocationPtr = &previousLocation;
    }
    return compiler_report_duplicate_type_binding(
            cs, name, location, previousLocationPtr);
}

TZrBool ZrParser_Compiler_ValidateVariableDeclaration(
        SZrCompilerState *cs,
        const SZrAstNode *declaration) {
    const SZrVariableDeclaration *variable;
    SZrStructuredDiagnostic diagnostic;
    SZrFileRange location;
    const TZrChar *message = "initializer requires annotation";

    if (cs == ZR_NULL || cs->state == ZR_NULL || declaration == ZR_NULL ||
        declaration->type != ZR_AST_VARIABLE_DECLARATION) {
        return ZR_FALSE;
    }

    variable = &declaration->data.variableDeclaration;
    if (variable->typeInfo != ZR_NULL || variable->value != ZR_NULL) {
        return ZR_TRUE;
    }

    location = variable->pattern != ZR_NULL
                       ? variable->pattern->location
                       : declaration->location;
    ZrParser_StructuredDiagnostic_Init(&diagnostic);
    if (!ZrParser_DiagnosticBuilder_Build(
                cs->state,
                &diagnostic,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "initializer_requires_annotation",
                message,
                "The declaration has neither an explicit type nor an initializer from which an exact type can be inferred.",
                "Add a type annotation or initialize the variable with an expression of the intended type.") ||
        !ZrParser_StructuredDiagnostic_SetNoFixReason(
                &diagnostic,
                ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION)) {
        ZrParser_StructuredDiagnostic_Free(cs->state, &diagnostic);
        ZrParser_Compiler_Error(cs, message, location);
        return ZR_FALSE;
    }

    ZrParser_Compiler_StructuredError(cs, &diagnostic);
    return ZR_FALSE;
}

TZrBool ZrParser_Compiler_ReportCannotInferExactType(
        SZrCompilerState *cs,
        SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;
    const TZrChar *message = "cannot infer exact type";

    if (cs == ZR_NULL || cs->state == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_StructuredDiagnostic_Init(&diagnostic);
    if (!ZrParser_DiagnosticBuilder_Build(
                cs->state,
                &diagnostic,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "cannot_infer_exact_type",
                message,
                "The compiler could not prove one canonical type for this declaration or expression.",
                "Add an explicit type annotation or rewrite the expression so one exact type can be inferred.") ||
        !ZrParser_StructuredDiagnostic_SetNoFixReason(
                &diagnostic,
                ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION)) {
        ZrParser_StructuredDiagnostic_Free(cs->state, &diagnostic);
        ZrParser_Compiler_Error(cs, message, location);
        return ZR_FALSE;
    }

    ZrParser_Compiler_StructuredError(cs, &diagnostic);
    return ZR_TRUE;
}

TZrBool ZrParser_Compiler_RegisterTypeBinding(
        SZrCompilerState *cs,
        SZrString *name,
        SZrFileRange location,
        SZrAstNode *declaration) {
    const SZrSemanticSymbolRecord *previous = ZR_NULL;
    const SZrFileRange *previousLocation = ZR_NULL;
    SZrString *previousName;
    SZrFileRange previousNameRange;

    if (cs == ZR_NULL || cs->state == ZR_NULL ||
        cs->typeEnv == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!ZrParser_TypeEnvironment_LookupType(cs->typeEnv, name)) {
        return ZrParser_TypeEnvironment_RegisterType(
                cs->state, cs->typeEnv, name);
    }

    if (cs->semanticContext != ZR_NULL &&
        cs->semanticContext->symbols.isValid) {
        for (TZrSize index = 0U;
             index < cs->semanticContext->symbols.length;
             index++) {
            const SZrSemanticSymbolRecord *candidate =
                    (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                            &cs->semanticContext->symbols,
                            index);

            if (candidate == ZR_NULL ||
                candidate->kind != ZR_SEMANTIC_SYMBOL_KIND_TYPE ||
                candidate->name == ZR_NULL ||
                !ZrCore_String_Equal(candidate->name, name)) {
                continue;
            }
            if (previous == ZR_NULL) {
                previous = candidate;
            }
            if (candidate->astNode != ZR_NULL &&
                candidate->astNode != declaration) {
                previous = candidate;
                break;
            }
        }
    }
    if (previous != ZR_NULL && previous->astNode != declaration) {
        if (compiler_duplicate_type_declaration_identity(
                    previous->astNode,
                    &previousName,
                    &previousNameRange) &&
            previousName != ZR_NULL &&
            ZrCore_String_Equal(previousName, name)) {
            previousLocation = &previousNameRange;
        } else {
            previousLocation = &previous->location;
        }
    }
    (void)compiler_report_duplicate_type_binding(
            cs, name, location, previousLocation);
    return ZR_FALSE;
}

void ZrParser_Compiler_PatternShapeMismatch(SZrCompilerState *cs,
                                            SZrFileRange location,
                                            const TZrChar *message,
                                            const TZrChar *cause,
                                            const TZrChar *suggestion) {
    SZrStructuredDiagnostic diagnostic;

    if (cs == ZR_NULL || cs->state == ZR_NULL) {
        return;
    }

    ZrParser_StructuredDiagnostic_Init(&diagnostic);
    if (!ZrParser_DiagnosticBuilder_BuildPatternShapeMismatch(cs->state,
                                                              &diagnostic,
                                                              location,
                                                              message,
                                                              cause,
                                                              suggestion)) {
        ZrParser_Compiler_Error(cs,
                                message != ZR_NULL
                                    ? message
                                    : "Union pattern destructuring shape does not match variant payload shape",
                                location);
        return;
    }

    ZrParser_Compiler_StructuredError(cs, &diagnostic);
}

void ZrParser_Compiler_PatternUnknownField(SZrCompilerState *cs,
                                           SZrFileRange location,
                                           const TZrChar *fieldName,
                                           const TZrChar *availableFields) {
    SZrStructuredDiagnostic diagnostic;

    if (cs == ZR_NULL || cs->state == ZR_NULL) {
        return;
    }

    ZrParser_StructuredDiagnostic_Init(&diagnostic);
    if (!ZrParser_DiagnosticBuilder_BuildPatternUnknownField(cs->state,
                                                             &diagnostic,
                                                             location,
                                                             fieldName,
                                                             availableFields)) {
        ZrParser_Compiler_Error(cs, "Unknown union pattern field", location);
        return;
    }

    ZrParser_Compiler_StructuredError(cs, &diagnostic);
}

void ZrParser_Compiler_PatternArityMismatch(SZrCompilerState *cs,
                                            SZrFileRange location,
                                            TZrSize expectedCount,
                                            TZrSize actualCount,
                                            const TZrChar *availableFields) {
    SZrStructuredDiagnostic diagnostic;

    if (cs == ZR_NULL || cs->state == ZR_NULL) {
        return;
    }

    ZrParser_StructuredDiagnostic_Init(&diagnostic);
    if (!ZrParser_DiagnosticBuilder_BuildPatternArityMismatch(cs->state,
                                                              &diagnostic,
                                                              location,
                                                              expectedCount,
                                                              actualCount,
                                                              availableFields)) {
        ZrParser_Compiler_Error(cs, "Union pattern arity mismatch", location);
        return;
    }

    ZrParser_Compiler_StructuredError(cs, &diagnostic);
}

void ZrParser_Compiler_PatternVariantMismatch(SZrCompilerState *cs,
                                              SZrFileRange location,
                                              const TZrChar *annotationUnionName,
                                              const TZrChar *variantName,
                                              const TZrChar *resourceUnionName) {
    SZrStructuredDiagnostic diagnostic;

    if (cs == ZR_NULL || cs->state == ZR_NULL) {
        return;
    }

    ZrParser_StructuredDiagnostic_Init(&diagnostic);
    if (!ZrParser_DiagnosticBuilder_BuildPatternVariantMismatch(cs->state,
                                                                &diagnostic,
                                                                location,
                                                                annotationUnionName,
                                                                variantName,
                                                                resourceUnionName)) {
        ZrParser_Compiler_Error(cs, "Using union pattern annotation does not match the resource union type", location);
        return;
    }

    ZrParser_Compiler_StructuredError(cs, &diagnostic);
}

static void compiler_buffer_appendf(TZrChar *buffer,
                                    TZrSize bufferSize,
                                    TZrSize *offset,
                                    const TZrChar *format,
                                    ...) {
    va_list args;
    int written;

    if (buffer == ZR_NULL || bufferSize == 0 || offset == ZR_NULL || *offset >= bufferSize || format == ZR_NULL) {
        return;
    }

    va_start(args, format);
    written = vsnprintf(buffer + *offset, bufferSize - *offset, format, args);
    va_end(args);
    if (written < 0) {
        return;
    }

    if ((TZrSize)written >= bufferSize - *offset) {
        *offset = bufferSize - 1;
        buffer[*offset] = '\0';
        return;
    }

    *offset += (TZrSize)written;
}

void print_error_suggestion(const TZrChar *msg) {
    ZR_UNUSED_PARAMETER(msg);
}

static void compiler_append_error_suggestion(TZrChar *buffer,
                                             TZrSize bufferSize,
                                             TZrSize *offset,
                                             const TZrChar *msg) {
    if (msg == ZR_NULL) {
        return;
    }

    if (strstr(msg, "INTERFACE_METHOD_SIGNATURE") != ZR_NULL ||
        strstr(msg, "INTERFACE_FIELD_DECLARATION") != ZR_NULL ||
        strstr(msg, "INTERFACE_PROPERTY_SIGNATURE") != ZR_NULL ||
        strstr(msg, "STRUCT_FIELD") != ZR_NULL ||
        strstr(msg, "STRUCT_METHOD") != ZR_NULL ||
        strstr(msg, "CLASS_FIELD") != ZR_NULL ||
        strstr(msg, "CLASS_METHOD") != ZR_NULL ||
        strstr(msg, "FUNCTION_DECLARATION") != ZR_NULL ||
        strstr(msg, "STRUCT_DECLARATION") != ZR_NULL ||
        strstr(msg, "CLASS_DECLARATION") != ZR_NULL ||
        strstr(msg, "INTERFACE_DECLARATION") != ZR_NULL ||
        strstr(msg, "ENUM_DECLARATION") != ZR_NULL ||
        strstr(msg, "cannot be used as") != ZR_NULL) {
        compiler_buffer_appendf(buffer,
                                bufferSize,
                                offset,
                                "  Suggestion: Declaration types (interface, struct, class, enum, function) cannot be used as statements or expressions.\n"
                                "              They should only appear in their proper declaration contexts (top-level, class body, etc.).\n"
                                "              Check if you accidentally placed a declaration inside a block or expression context.\n");
    } else if (strstr(msg, "Unexpected expression type") != ZR_NULL) {
        compiler_buffer_appendf(buffer,
                                bufferSize,
                                offset,
                                "  Suggestion: This AST node type is not supported in expression context.\n"
                                "              Possible causes:\n"
                                "              1. The node was incorrectly parsed or placed in the wrong context\n"
                                "              2. A declaration type was mistakenly used as an expression\n"
                                "              3. Missing implementation for this node type in ZrParser_Expression_Compile\n"
                                "              Check the AST structure and ensure the node is in the correct context.\n");
    } else if (strstr(msg, "not found") != ZR_NULL) {
        compiler_buffer_appendf(buffer,
                                bufferSize,
                                offset,
                                "  Suggestion: The referenced identifier was not found. Check:\n"
                                "              1. Variable/function is declared before use\n"
                                "              2. Variable/function is in scope\n"
                                "              3. Spelling is correct\n"
                                "              4. Import statements are correct (if using modules)\n");
    } else if (strstr(msg, "Destructuring") != ZR_NULL) {
        compiler_buffer_appendf(buffer,
                                bufferSize,
                                offset,
                                "  Suggestion: Destructuring patterns can only be used in variable declarations.\n"
                                "              They cannot be used as standalone expressions or statements.\n");
    } else if (strstr(msg, "Loop or statement") != ZR_NULL) {
        compiler_buffer_appendf(buffer,
                                bufferSize,
                                offset,
                                "  Suggestion: Control flow statements (if, while, for, etc.) cannot be used as expressions.\n"
                                "              Use them as statements, or use expression forms (if expression, etc.) if available.\n");
    } else if (strstr(msg, "Failed to") != ZR_NULL) {
        compiler_buffer_appendf(buffer,
                                bufferSize,
                                offset,
                                "  Suggestion: Internal compiler error. This may indicate:\n"
                                "              1. Memory allocation failure\n"
                                "              2. Invalid compiler state\n"
                                "              3. Bug in the compiler\n"
                                "              Please report this issue with the source code that triggered it.\n");
    }
}

static void compiler_append_error_analysis(TZrChar *buffer,
                                           TZrSize bufferSize,
                                           TZrSize *offset,
                                           const TZrChar *msg) {
    compiler_buffer_appendf(buffer, bufferSize, offset, "\n  Error Analysis:\n");
    if (strstr(msg, "INTERFACE_METHOD_SIGNATURE") != ZR_NULL ||
        strstr(msg, "INTERFACE_FIELD_DECLARATION") != ZR_NULL ||
        strstr(msg, "INTERFACE_PROPERTY_SIGNATURE") != ZR_NULL) {
        compiler_buffer_appendf(buffer,
                                bufferSize,
                                offset,
                                "    - Problem: Interface declaration member found in invalid context\n"
                                "    - Root Cause: Interface members (methods, fields, properties) can only appear\n"
                                "                  inside interface declaration bodies, not in statements or expressions\n");
    } else if (strstr(msg, "STRUCT_FIELD") != ZR_NULL ||
               strstr(msg, "STRUCT_METHOD") != ZR_NULL ||
               strstr(msg, "CLASS_FIELD") != ZR_NULL ||
               strstr(msg, "CLASS_METHOD") != ZR_NULL) {
        compiler_buffer_appendf(buffer,
                                bufferSize,
                                offset,
                                "    - Problem: Struct/Class member found in invalid context\n"
                                "    - Root Cause: Struct/Class members can only appear inside struct/class\n"
                                "                  declaration bodies, not in statements or expressions\n");
    } else if (strstr(msg, "Unexpected expression type") != ZR_NULL) {
        compiler_buffer_appendf(buffer,
                                bufferSize,
                                offset,
                                "    - Problem: AST node type not supported in expression context\n"
                                "    - Root Cause: The compiler encountered a node type that cannot be compiled\n"
                                "                  as an expression. This may indicate a parsing error or missing\n"
                                "                  implementation for this node type.\n");
    } else if (strstr(msg, "not found") != ZR_NULL) {
        compiler_buffer_appendf(buffer,
                                bufferSize,
                                offset,
                                "    - Problem: Identifier not found in current scope\n"
                                "    - Root Cause: The variable, function, or type name was not found in the\n"
                                "                  current scope or type environment\n");
    } else {
        compiler_buffer_appendf(buffer,
                                bufferSize,
                                offset,
                                "    - Problem: Compilation error occurred\n"
                                "    - Root Cause: See error message above for details\n");
    }
}

// 编译期错误报告
void ZrParser_CompileTime_Error(SZrCompilerState *cs, EZrCompileTimeErrorLevel level, const TZrChar *message, SZrFileRange location) {
    EZrLogLevel logLevel = ZR_LOG_LEVEL_INFO;

    if (cs == ZR_NULL || message == ZR_NULL) {
        return;
    }
    
    const TZrChar *levelStr = "INFO";
    switch (level) {
        case ZR_COMPILE_TIME_ERROR_INFO:
            levelStr = "INFO";
            logLevel = ZR_LOG_LEVEL_INFO;
            break;
        case ZR_COMPILE_TIME_ERROR_WARNING:
            levelStr = "WARNING";
            logLevel = ZR_LOG_LEVEL_WARNING;
            break;
        case ZR_COMPILE_TIME_ERROR_ERROR:
            levelStr = "ERROR";
            logLevel = ZR_LOG_LEVEL_ERROR;
            cs->hasError = ZR_TRUE;
            cs->hadRecoverableError = ZR_TRUE;
            cs->hasCompileTimeError = ZR_TRUE;
            break;
        case ZR_COMPILE_TIME_ERROR_FATAL:
            levelStr = "FATAL";
            logLevel = ZR_LOG_LEVEL_FATAL;
            cs->hasError = ZR_TRUE;
            cs->hadRecoverableError = ZR_TRUE;
            cs->hasCompileTimeError = ZR_TRUE;
            break;
    }
    
    // 输出错误信息
    const TZrChar *fileName = "<unknown>";
    if (location.source != ZR_NULL) {
        TZrNativeString nameStr = ZrCore_String_GetNativeString(location.source);
        if (nameStr != ZR_NULL) {
            fileName = nameStr;
        }
    }
    
    if (!cs->suppressErrorOutput) {
        ZrCore_Log_Diagnosticf(cs->state,
                               logLevel,
                               ZR_OUTPUT_CHANNEL_STDERR,
                               "[CompileTime %s] %s:%d:%d: %s\n",
                               levelStr,
                               fileName,
                               location.start.line,
                               location.start.column,
                               message);
    }
    
    // 如果是致命错误，设置错误信息
    if (level == ZR_COMPILE_TIME_ERROR_ERROR || level == ZR_COMPILE_TIME_ERROR_FATAL) {
        if (cs->errorMessage == ZR_NULL) {
            compiler_store_error_message(cs, message);
            cs->errorLocation = location;
        }
    }

    if (level == ZR_COMPILE_TIME_ERROR_FATAL) {
        cs->hasFatalError = ZR_TRUE;
    }
}

static void compiler_report_error(SZrCompilerState *cs,
                                  const TZrChar *msg,
                                  SZrFileRange location,
                                  TZrBool clearStructuredError) {
    const TZrChar *structuredCause = ZR_NULL;
    const TZrChar *structuredSuggestion = ZR_NULL;

    if (cs == ZR_NULL) {
        return;
    }

    if (clearStructuredError) {
        ZrParser_Compiler_ClearStructuredError(cs);
    }
    cs->hasError = ZR_TRUE;
    cs->hadRecoverableError = ZR_TRUE;
    compiler_store_error_message(cs, msg);
    cs->errorLocation = location;

    // 输出详细的错误信息（包含行列号）
    const TZrChar *sourceName = "unknown";
    TZrSize nameLen = 7; // "unknown" 的长度
    TZrChar messageBuffer[4096];
    TZrSize offset = 0;
    if (location.source != ZR_NULL) {
        if (location.source->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
            sourceName = ZrCore_String_GetNativeStringShort(location.source);
            nameLen = location.source->shortStringLength;
        } else {
            sourceName = ZrCore_String_GetNativeString(location.source);
            nameLen = location.source->longStringLength;
        }
    }

    compiler_buffer_appendf(messageBuffer, sizeof(messageBuffer), &offset, "\n");
    compiler_buffer_appendf(messageBuffer,
                            sizeof(messageBuffer),
                            &offset,
                            "═══════════════════════════════════════════════════════════════\n"
                            "Compiler Error\n"
                            "═══════════════════════════════════════════════════════════════\n"
                            "  Error Message: %s\n"
                            "  Location: %.*s:%d:%d - %d:%d\n",
                            msg,
                            (int)nameLen,
                            sourceName,
                            location.start.line,
                            location.start.column,
                            location.end.line,
                            location.end.column);
    if (!clearStructuredError && cs->hasStructuredError) {
        if (cs->structuredError.cause != ZR_NULL) {
            structuredCause = cs->structuredError.cause->shortStringLength < ZR_VM_LONG_STRING_FLAG
                                      ? ZrCore_String_GetNativeStringShort(cs->structuredError.cause)
                                      : ZrCore_String_GetNativeString(cs->structuredError.cause);
        }
        if (cs->structuredError.suggestion != ZR_NULL) {
            structuredSuggestion = cs->structuredError.suggestion->shortStringLength < ZR_VM_LONG_STRING_FLAG
                                           ? ZrCore_String_GetNativeStringShort(cs->structuredError.suggestion)
                                           : ZrCore_String_GetNativeString(cs->structuredError.suggestion);
        }
    }
    if (structuredCause != ZR_NULL) {
        compiler_buffer_appendf(messageBuffer,
                                sizeof(messageBuffer),
                                &offset,
                                "\n  Error Analysis:\n    - Root Cause: %s\n",
                                structuredCause);
    } else {
        compiler_append_error_analysis(messageBuffer, sizeof(messageBuffer), &offset, msg);
    }
    compiler_buffer_appendf(messageBuffer, sizeof(messageBuffer), &offset, "\n  How to Fix:\n");
    if (structuredSuggestion != ZR_NULL) {
        compiler_buffer_appendf(messageBuffer,
                                sizeof(messageBuffer),
                                &offset,
                                "  Suggestion: %s\n",
                                structuredSuggestion);
    } else {
        compiler_append_error_suggestion(messageBuffer, sizeof(messageBuffer), &offset, msg);
    }
    compiler_buffer_appendf(messageBuffer,
                            sizeof(messageBuffer),
                            &offset,
                            "═══════════════════════════════════════════════════════════════\n\n");
    if (!cs->suppressErrorOutput) {
        ZrCore_Log_Diagnosticf(cs->state,
                               ZR_LOG_LEVEL_ERROR,
                               ZR_OUTPUT_CHANNEL_STDERR,
                               "%s",
                               messageBuffer);
    }
}

void ZrParser_Compiler_Error(SZrCompilerState *cs, const TZrChar *msg, SZrFileRange location) {
    compiler_report_error(cs, msg, location, ZR_TRUE);
}

// 创建指令（辅助函数）
