//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"

#include <stdarg.h>

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
    } else if (strstr(msg, "Type mismatch") != ZR_NULL ||
               strstr(msg, "Incompatible types") != ZR_NULL) {
        compiler_buffer_appendf(buffer,
                                bufferSize,
                                offset,
                                "  Suggestion: Type mismatch detected. Check:\n"
                                "              1. Variable types match their assignments\n"
                                "              2. Function argument types match the function signature\n"
                                "              3. Return types match the function declaration\n"
                                "              4. Type annotations are correct\n");
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
    } else if (strstr(msg, "Type mismatch") != ZR_NULL ||
               strstr(msg, "Incompatible types") != ZR_NULL) {
        compiler_buffer_appendf(buffer,
                                bufferSize,
                                offset,
                                "    - Problem: Type compatibility check failed\n"
                                "    - Root Cause: The types of operands, variables, or function arguments are\n"
                                "                  not compatible with the operation or assignment being performed\n");
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
    if (cs == ZR_NULL || message == ZR_NULL) {
        return;
    }
    
    const TZrChar *levelStr = "INFO";
    switch (level) {
        case ZR_COMPILE_TIME_ERROR_INFO:
            levelStr = "INFO";
            break;
        case ZR_COMPILE_TIME_ERROR_WARNING:
            levelStr = "WARNING";
            break;
        case ZR_COMPILE_TIME_ERROR_ERROR:
            levelStr = "ERROR";
            cs->hasError = ZR_TRUE;
            cs->hadRecoverableError = ZR_TRUE;
            cs->hasCompileTimeError = ZR_TRUE;
            break;
        case ZR_COMPILE_TIME_ERROR_FATAL:
            levelStr = "FATAL";
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
    
    ZrCore_Log_Diagnosticf(cs->state,
                           level == ZR_COMPILE_TIME_ERROR_FATAL ? ZR_LOG_LEVEL_FATAL : ZR_LOG_LEVEL_ERROR,
                           ZR_OUTPUT_CHANNEL_STDERR,
                           "[CompileTime %s] %s:%d:%d: %s\n",
                           levelStr,
                           fileName,
                           location.start.line,
                           location.start.column,
                           message);
    
    // 如果是致命错误，设置错误信息
    if (level == ZR_COMPILE_TIME_ERROR_ERROR || level == ZR_COMPILE_TIME_ERROR_FATAL) {
        if (cs->errorMessage == ZR_NULL) {
            compiler_store_error_message(cs,
                                         (level == ZR_COMPILE_TIME_ERROR_FATAL)
                                                 ? "Fatal compile-time evaluation failed"
                                                 : "Compile-time evaluation failed");
            cs->errorLocation = location;
        }
    }

    if (level == ZR_COMPILE_TIME_ERROR_FATAL) {
        cs->hasFatalError = ZR_TRUE;
    }
}

void ZrParser_Compiler_Error(SZrCompilerState *cs, const TZrChar *msg, SZrFileRange location) {
    if (cs == ZR_NULL) {
        return;
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
    compiler_append_error_analysis(messageBuffer, sizeof(messageBuffer), &offset, msg);
    compiler_buffer_appendf(messageBuffer, sizeof(messageBuffer), &offset, "\n  How to Fix:\n");
    compiler_append_error_suggestion(messageBuffer, sizeof(messageBuffer), &offset, msg);
    compiler_buffer_appendf(messageBuffer,
                            sizeof(messageBuffer),
                            &offset,
                            "═══════════════════════════════════════════════════════════════\n\n");
    ZrCore_Log_Diagnosticf(cs->state,
                           ZR_LOG_LEVEL_ERROR,
                           ZR_OUTPUT_CHANNEL_STDERR,
                           "%s",
                           messageBuffer);
}

// 创建指令（辅助函数）
