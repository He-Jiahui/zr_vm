//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"

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

void print_error_suggestion(const TZrChar *msg) {
    if (msg == ZR_NULL) {
        return;
    }
    
    // 根据错误消息内容提供解决建议
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
        fprintf(stderr, "  Suggestion: Declaration types (interface, struct, class, enum, function) cannot be used as statements or expressions.\n");
        fprintf(stderr, "              They should only appear in their proper declaration contexts (top-level, class body, etc.).\n");
        fprintf(stderr, "              Check if you accidentally placed a declaration inside a block or expression context.\n");
    } else if (strstr(msg, "Unexpected expression type") != ZR_NULL) {
        fprintf(stderr, "  Suggestion: This AST node type is not supported in expression context.\n");
        fprintf(stderr, "              Possible causes:\n");
        fprintf(stderr, "              1. The node was incorrectly parsed or placed in the wrong context\n");
        fprintf(stderr, "              2. A declaration type was mistakenly used as an expression\n");
        fprintf(stderr, "              3. Missing implementation for this node type in ZrParser_Expression_Compile\n");
        fprintf(stderr, "              Check the AST structure and ensure the node is in the correct context.\n");
    } else if (strstr(msg, "Type mismatch") != ZR_NULL ||
               strstr(msg, "Incompatible types") != ZR_NULL) {
        fprintf(stderr, "  Suggestion: Type mismatch detected. Check:\n");
        fprintf(stderr, "              1. Variable types match their assignments\n");
        fprintf(stderr, "              2. Function argument types match the function signature\n");
        fprintf(stderr, "              3. Return types match the function declaration\n");
        fprintf(stderr, "              4. Type annotations are correct\n");
    } else if (strstr(msg, "not found") != ZR_NULL) {
        fprintf(stderr, "  Suggestion: The referenced identifier was not found. Check:\n");
        fprintf(stderr, "              1. Variable/function is declared before use\n");
        fprintf(stderr, "              2. Variable/function is in scope\n");
        fprintf(stderr, "              3. Spelling is correct\n");
        fprintf(stderr, "              4. Import statements are correct (if using modules)\n");
    } else if (strstr(msg, "Destructuring") != ZR_NULL) {
        fprintf(stderr, "  Suggestion: Destructuring patterns can only be used in variable declarations.\n");
        fprintf(stderr, "              They cannot be used as standalone expressions or statements.\n");
    } else if (strstr(msg, "Loop or statement") != ZR_NULL) {
        fprintf(stderr, "  Suggestion: Control flow statements (if, while, for, etc.) cannot be used as expressions.\n");
        fprintf(stderr, "              Use them as statements, or use expression forms (if expression, etc.) if available.\n");
    } else if (strstr(msg, "Failed to") != ZR_NULL) {
        fprintf(stderr, "  Suggestion: Internal compiler error. This may indicate:\n");
        fprintf(stderr, "              1. Memory allocation failure\n");
        fprintf(stderr, "              2. Invalid compiler state\n");
        fprintf(stderr, "              3. Bug in the compiler\n");
        fprintf(stderr, "              Please report this issue with the source code that triggered it.\n");
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
    
    fprintf(stderr, "[CompileTime %s] %s:%d:%d: %s\n", 
           levelStr, fileName, location.start.line, location.start.column, message);
    
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
    if (location.source != ZR_NULL) {
        if (location.source->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
            sourceName = ZrCore_String_GetNativeStringShort(location.source);
            nameLen = location.source->shortStringLength;
        } else {
            sourceName = ZrCore_String_GetNativeString(location.source);
            nameLen = location.source->longStringLength;
        }
    }

    // 输出格式化的错误信息
    fprintf(stderr, "\n");
    fprintf(stderr, "═══════════════════════════════════════════════════════════════\n");
    fprintf(stderr, "Compiler Error\n");
    fprintf(stderr, "═══════════════════════════════════════════════════════════════\n");
    fprintf(stderr, "  Error Message: %s\n", msg);
    fprintf(stderr, "  Location: %.*s:%d:%d - %d:%d\n", 
           (int) nameLen, sourceName, 
           location.start.line, location.start.column,
           location.end.line, location.end.column);
    
    // 输出错误原因分析
    fprintf(stderr, "\n  Error Analysis:\n");
    if (strstr(msg, "INTERFACE_METHOD_SIGNATURE") != ZR_NULL ||
        strstr(msg, "INTERFACE_FIELD_DECLARATION") != ZR_NULL ||
        strstr(msg, "INTERFACE_PROPERTY_SIGNATURE") != ZR_NULL) {
        fprintf(stderr, "    - Problem: Interface declaration member found in invalid context\n");
        fprintf(stderr, "    - Root Cause: Interface members (methods, fields, properties) can only appear\n");
        fprintf(stderr, "                  inside interface declaration bodies, not in statements or expressions\n");
    } else if (strstr(msg, "STRUCT_FIELD") != ZR_NULL ||
               strstr(msg, "STRUCT_METHOD") != ZR_NULL ||
               strstr(msg, "CLASS_FIELD") != ZR_NULL ||
               strstr(msg, "CLASS_METHOD") != ZR_NULL) {
        fprintf(stderr, "    - Problem: Struct/Class member found in invalid context\n");
        fprintf(stderr, "    - Root Cause: Struct/Class members can only appear inside struct/class\n");
        fprintf(stderr, "                  declaration bodies, not in statements or expressions\n");
    } else if (strstr(msg, "Unexpected expression type") != ZR_NULL) {
        fprintf(stderr, "    - Problem: AST node type not supported in expression context\n");
        fprintf(stderr, "    - Root Cause: The compiler encountered a node type that cannot be compiled\n");
        fprintf(stderr, "                  as an expression. This may indicate a parsing error or missing\n");
        fprintf(stderr, "                  implementation for this node type.\n");
    } else if (strstr(msg, "Type mismatch") != ZR_NULL ||
               strstr(msg, "Incompatible types") != ZR_NULL) {
        fprintf(stderr, "    - Problem: Type compatibility check failed\n");
        fprintf(stderr, "    - Root Cause: The types of operands, variables, or function arguments are\n");
        fprintf(stderr, "                  not compatible with the operation or assignment being performed\n");
    } else if (strstr(msg, "not found") != ZR_NULL) {
        fprintf(stderr, "    - Problem: Identifier not found in current scope\n");
        fprintf(stderr, "    - Root Cause: The variable, function, or type name was not found in the\n");
        fprintf(stderr, "                  current scope or type environment\n");
    } else {
        fprintf(stderr, "    - Problem: Compilation error occurred\n");
        fprintf(stderr, "    - Root Cause: See error message above for details\n");
    }
    
    // 输出解决建议
    fprintf(stderr, "\n  How to Fix:\n");
    print_error_suggestion(msg);
    
    fprintf(stderr, "═══════════════════════════════════════════════════════════════\n");
    fprintf(stderr, "\n");
}

// 创建指令（辅助函数）
