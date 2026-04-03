//
// Created by Auto on 2025/01/XX.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "unity.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser.h"

// 测试时间测量结构
typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

// 测试日志宏（符合测试规范）
#define TEST_START(summary)                                                                                            \
    do {                                                                                                               \
        printf("Unit Test - %s\n", summary);                                                                           \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_INFO(summary, details)                                                                                    \
    do {                                                                                                               \
        printf("Testing %s:\n %s\n", summary, details);                                                                \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_PASS_CUSTOM(timer, summary)                                                                               \
    do {                                                                                                               \
        double elapsed = ((double) (timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0;                       \
        printf("Pass - Cost Time:%.3fms - %s\n", elapsed, summary);                                                    \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_FAIL_CUSTOM(timer, summary, reason)                                                                       \
    do {                                                                                                               \
        clock_t failureTime = clock();                                                                                 \
        double elapsed = ((double) (failureTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0;                        \
        printf("Fail - Cost Time:%.3fms - %s:\n %s\n", elapsed, summary, reason);                                      \
        fflush(stdout);                                                                                                \
        TEST_FAIL_MESSAGE(reason);                                                                                     \
    } while (0)

#define TEST_DIVIDER()                                                                                                 \
    do {                                                                                                               \
        printf("----------\n");                                                                                        \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_MODULE_DIVIDER()                                                                                          \
    do {                                                                                                               \
        printf("==========\n");                                                                                        \
        fflush(stdout);                                                                                                \
    } while (0)

// 简单的测试分配器
static TZrPtr test_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        if (pointer != ZR_NULL && (TZrPtr) pointer >= (TZrPtr) 0x1000) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    } else {
        if ((TZrPtr) pointer >= (TZrPtr) 0x1000) {
            return realloc(pointer, newSize);
        } else {
            return malloc(newSize);
        }
    }
}

// 创建测试用的SZrState
static SZrState *create_test_state(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 0, &callbacks);
    if (global == ZR_NULL) {
        return ZR_NULL;
    }

    SZrState *state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);

    return state;
}

// 辅助函数：编译源代码并检查是否有错误
// 返回编译后的函数，如果编译失败则返回 ZR_NULL
// 通过 outHasError 参数返回是否有编译错误
// 注意：由于 ZrParser_Compiler_Compile 在 hasError 时返回 ZR_NULL，我们通过检查返回值来判断是否有错误
static SZrFunction *compile_source_and_check_error(SZrState *state, const TZrChar *source, TZrSize sourceLength, SZrString *sourceName, TZrBool *outHasError) {
    if (state == ZR_NULL || source == ZR_NULL || sourceLength == 0 || outHasError == ZR_NULL) {
        if (outHasError != ZR_NULL) {
            *outHasError = ZR_TRUE;
        }
        return ZR_NULL;
    }
    
    // 解析源代码为AST
    SZrAstNode *ast = ZrParser_Parse(state, source, sourceLength, sourceName);
    if (ast == ZR_NULL) {
        *outHasError = ZR_TRUE;
        return ZR_NULL;
    }
    
    // 编译AST为函数
    // 注意：ZrParser_Compiler_Compile 在 cs.hasError 为真时会返回 ZR_NULL
    // 所以如果 func == ZR_NULL，表示编译有错误
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);
    
    // 检查编译错误（通过检查函数是否为NULL，因为ZrCompilerCompile在hasError时返回NULL）
    *outHasError = (func == ZR_NULL);
    
    // 释放AST
    ZrParser_Ast_Free(state, ast);
    
    return func;
}

// 销毁测试用的SZrState
static void destroy_test_state(SZrState *state) {
    if (state != ZR_NULL && state->global != ZR_NULL) {
        ZrCore_GlobalState_Free(state->global);
    }
}

// 测试 1: 局部 const 变量声明和赋值
static void test_const_local_variable_declaration(void) {
    TEST_START("Const Local Variable Declaration");
    SZrTestTimer timer;
    timer.startTime = clock();
    
    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, "Const Local Variable Declaration", "Failed to create test state");
        return;
    }
    
    const char *source = "var const a: int = 1;";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_local.zr");
    SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    
    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Const Local Variable Declaration", "Failed to compile const local variable");
        destroy_test_state(state);
        return;
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Local Variable Declaration");
    destroy_test_state(state);
}

// 测试 2: 局部 const 变量后续赋值（应报错）
static void test_const_local_variable_reassignment_error(void) {
    TEST_START("Const Local Variable Reassignment Error");
    SZrTestTimer timer;
    timer.startTime = clock();
    
    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, "Const Local Variable Reassignment Error", "Failed to create test state");
        return;
    }
    
    const char *source = "var const a: int = 1; a = 2;";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_local_reassign.zr");
    TZrBool hasError = ZR_FALSE;
    SZrFunction *func = compile_source_and_check_error(state, source, strlen(source), sourceName, &hasError);
    
    // 应该编译失败（const 变量不能重新赋值）
    if (!hasError || func != ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Const Local Variable Reassignment Error", "Expected compilation error but compilation succeeded");
        if (func != ZR_NULL) {
            // 清理函数对象（如果存在）
        }
        destroy_test_state(state);
        return;
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Local Variable Reassignment Error");
    destroy_test_state(state);
}

// 测试 3: 类 const 字段在构造函数中赋值
static void test_const_class_field_in_constructor(void) {
    TEST_START("Const Class Field in Constructor");
    SZrTestTimer timer;
    timer.startTime = clock();
    
    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, "Const Class Field in Constructor", "Failed to create test state");
        return;
    }
    
    const char *source = 
        "class MyClass {\n"
        "    pub const id: int;\n"
        "    pub @constructor() {\n"
        "        this.id = 1;\n"
        "    }\n"
        "}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_class_constructor.zr");
    SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    
    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Const Class Field in Constructor", "Failed to compile const class field in constructor");
        destroy_test_state(state);
        return;
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Class Field in Constructor");
    destroy_test_state(state);
}

// 测试 4: 类 const 字段在普通方法中赋值（应报错）
static void test_const_class_field_in_method_error(void) {
    TEST_START("Const Class Field in Method Error");
    SZrTestTimer timer;
    timer.startTime = clock();
    
    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, "Const Class Field in Method Error", "Failed to create test state");
        return;
    }
    
    const char *source = 
        "class MyClass {\n"
        "    pub const id: int;\n"
        "    pub @constructor() {\n"
        "        this.id = 1;\n"
        "    }\n"
        "    pub update() {\n"
        "        this.id = 2;\n"
        "    }\n"
        "}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_class_method.zr");
    TZrBool hasError = ZR_FALSE;
    SZrFunction *func = compile_source_and_check_error(state, source, strlen(source), sourceName, &hasError);
    
    // 应该编译失败（const 字段不能在构造函数外赋值）
    if (!hasError || func != ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Const Class Field in Method Error", "Expected compilation error but compilation succeeded");
        if (func != ZR_NULL) {
            // 清理函数对象（如果存在）
        }
        destroy_test_state(state);
        return;
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Class Field in Method Error");
    destroy_test_state(state);
}

// 测试 5: 结构体 const 字段在构造函数中赋值
static void test_const_struct_field_in_constructor(void) {
    TEST_START("Const Struct Field in Constructor");
    SZrTestTimer timer;
    timer.startTime = clock();
    
    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, "Const Struct Field in Constructor", "Failed to create test state");
        return;
    }
    
    const char *source = 
        "struct MyStruct {\n"
        "    pub const id: int;\n"
        "    pub @constructor() {\n"
        "        this.id = 1;\n"
        "    }\n"
        "}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_struct_constructor.zr");
    SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    
    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Const Struct Field in Constructor", "Failed to compile const struct field in constructor");
        destroy_test_state(state);
        return;
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Struct Field in Constructor");
    destroy_test_state(state);
}

// 测试 6: 结构体 const 字段在普通方法中赋值（应报错）
static void test_const_struct_field_in_method_error(void) {
    TEST_START("Const Struct Field in Method Error");
    SZrTestTimer timer;
    timer.startTime = clock();
    
    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, "Const Struct Field in Method Error", "Failed to create test state");
        return;
    }
    
    const char *source = 
        "struct MyStruct {\n"
        "    pub const id: int;\n"
        "    pub @constructor() {\n"
        "        this.id = 1;\n"
        "    }\n"
        "    pub update() {\n"
        "        this.id = 2;\n"
        "    }\n"
        "}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_struct_method.zr");
    TZrBool hasError = ZR_FALSE;
    SZrFunction *func = compile_source_and_check_error(state, source, strlen(source), sourceName, &hasError);
    
    // 应该编译失败（const 字段不能在构造函数外赋值）
    if (!hasError || func != ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Const Struct Field in Method Error", "Expected compilation error but compilation succeeded");
        if (func != ZR_NULL) {
            // 清理函数对象（如果存在）
        }
        destroy_test_state(state);
        return;
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Struct Field in Method Error");
    destroy_test_state(state);
}

// 测试 7: 接口 const 字段声明
static void test_const_interface_field_declaration(void) {
    TEST_START("Const Interface Field Declaration");
    SZrTestTimer timer;
    timer.startTime = clock();
    
    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, "Const Interface Field Declaration", "Failed to create test state");
        return;
    }
    
    const char *source = 
        "interface MyInterface {\n"
        "    pub const version: int;\n"
        "}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_interface.zr");
    SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    
    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Const Interface Field Declaration", "Failed to compile const interface field");
        destroy_test_state(state);
        return;
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Interface Field Declaration");
    destroy_test_state(state);
}

// 测试 8: 类实现接口时 const 字段匹配检查
static void test_const_interface_implementation_match(void) {
    TEST_START("Const Interface Implementation Match");
    SZrTestTimer timer;
    timer.startTime = clock();
    
    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, "Const Interface Implementation Match", "Failed to create test state");
        return;
    }
    
    const char *source = 
        "interface MyInterface {\n"
        "    pub const version: int;\n"
        "}\n"
        "class MyClass: MyInterface {\n"
        "    pub const version: int;\n"
        "    pub @constructor() {\n"
        "        this.version = 1;\n"
        "    }\n"
        "}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_interface_impl.zr");
    SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    
    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Const Interface Implementation Match", "Failed to compile const interface implementation");
        destroy_test_state(state);
        return;
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Interface Implementation Match");
    destroy_test_state(state);
}

// 测试 9: 函数 const 参数声明
static void test_const_function_parameter(void) {
    TEST_START("Const Function Parameter");
    SZrTestTimer timer;
    timer.startTime = clock();
    
    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, "Const Function Parameter", "Failed to create test state");
        return;
    }
    
    const char *source = 
        "func process(const data: int) {\n"
        "    return data;\n"
        "}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_parameter.zr");
    SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    
    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Const Function Parameter", "Failed to compile const function parameter");
        destroy_test_state(state);
        return;
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Function Parameter");
    destroy_test_state(state);
}

// 测试 10: 函数内修改 const 参数（应报错）
static void test_const_parameter_modification_error(void) {
    TEST_START("Const Parameter Modification Error");
    SZrTestTimer timer;
    timer.startTime = clock();
    
    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, "Const Parameter Modification Error", "Failed to create test state");
        return;
    }
    
    const char *source = 
        "func process(const data: int) {\n"
        "    data = 2;\n"
        "    return data;\n"
        "}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_parameter_modify.zr");
    TZrBool hasError = ZR_FALSE;
    SZrFunction *func = compile_source_and_check_error(state, source, strlen(source), sourceName, &hasError);
    
    // 应该编译失败（const 参数不能修改）
    if (!hasError || func != ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Const Parameter Modification Error", "Expected compilation error but compilation succeeded");
        if (func != ZR_NULL) {
            // 清理函数对象（如果存在）
        }
        destroy_test_state(state);
        return;
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Parameter Modification Error");
    destroy_test_state(state);
}

// 测试 11: 静态 const 字段声明和初始化
static void test_const_static_field_declaration(void) {
    TEST_START("Const Static Field Declaration");
    SZrTestTimer timer;
    timer.startTime = clock();
    
    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, "Const Static Field Declaration", "Failed to create test state");
        return;
    }
    
    const char *source = 
        "class MyClass {\n"
        "    static const MAX_SIZE: int = 100;\n"
        "}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_static.zr");
    SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    
    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Const Static Field Declaration", "Failed to compile const static field");
        destroy_test_state(state);
        return;
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Static Field Declaration");
    destroy_test_state(state);
}

// 测试 12: 静态 const 字段后续赋值（应报错）
static void test_const_static_field_reassignment_error(void) {
    TEST_START("Const Static Field Reassignment Error");
    SZrTestTimer timer;
    timer.startTime = clock();
    
    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, "Const Static Field Reassignment Error", "Failed to create test state");
        return;
    }
    
    const char *source = 
        "class MyClass {\n"
        "    static const MAX_SIZE: int = 100;\n"
        "    pub update() {\n"
        "        MyClass.MAX_SIZE = 200;\n"
        "    }\n"
        "}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_static_reassign.zr");
    TZrBool hasError = ZR_FALSE;
    SZrFunction *func = compile_source_and_check_error(state, source, strlen(source), sourceName, &hasError);
    
    // 应该编译失败（静态 const 字段不能重新赋值）
    if (!hasError || func != ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Const Static Field Reassignment Error", "Expected compilation error but compilation succeeded");
        if (func != ZR_NULL) {
            // 清理函数对象（如果存在）
        }
        destroy_test_state(state);
        return;
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Static Field Reassignment Error");
    destroy_test_state(state);
}

// 测试 13: 接口 const 字段实现不匹配（应报错）
static void test_const_interface_implementation_mismatch_error(void) {
    TEST_START("Const Interface Implementation Mismatch Error");
    SZrTestTimer timer;
    timer.startTime = clock();
    
    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, "Const Interface Implementation Mismatch Error", "Failed to create test state");
        return;
    }
    
    const char *source = 
        "interface MyInterface {\n"
        "    pub const version: int;\n"
        "}\n"
        "class MyClass: MyInterface {\n"
        "    pub var version: int;  // 不是 const，应该报错\n"
        "    pub @constructor() {\n"
        "        this.version = 1;\n"
        "    }\n"
        "}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_interface_mismatch.zr");
    TZrBool hasError = ZR_FALSE;
    SZrFunction *func = compile_source_and_check_error(state, source, strlen(source), sourceName, &hasError);
    
    // 应该编译失败（接口 const 字段在实现类中也必须是 const）
    if (!hasError || func != ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Const Interface Implementation Mismatch Error", "Expected compilation error but compilation succeeded");
        if (func != ZR_NULL) {
            // 清理函数对象（如果存在）
        }
        destroy_test_state(state);
        return;
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Interface Implementation Mismatch Error");
    destroy_test_state(state);
}

// 测试 14: const 字段在构造函数中多次赋值（应报错）
static void test_const_field_multiple_assignment_in_constructor(void) {
    TEST_START("Const Field Multiple Assignment in Constructor");
    SZrTestTimer timer;
    timer.startTime = clock();
    
    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, "Const Field Multiple Assignment in Constructor", "Failed to create test state");
        return;
    }
    
    const char *source = 
        "class MyClass {\n"
        "    pub const id: int;\n"
        "    pub @constructor() {\n"
        "        this.id = 1;\n"
        "        this.id = 2;\n"
        "    }\n"
        "}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_multiple_assign.zr");
    TZrBool hasError = ZR_FALSE;
    SZrFunction *func = compile_source_and_check_error(state, source, strlen(source), sourceName, &hasError);
    
    if (!hasError || func != ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer,
                         "Const Field Multiple Assignment in Constructor",
                         "Expected compilation error but compilation succeeded");
        destroy_test_state(state);
        return;
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Field Multiple Assignment in Constructor");
    destroy_test_state(state);
}

// 测试 15: const 局部变量未初始化（应报错）
static void test_const_local_variable_uninitialized_error(void) {
    TEST_START("Const Local Variable Uninitialized Error");
    SZrTestTimer timer;
    timer.startTime = clock();
    
    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, "Const Local Variable Uninitialized Error", "Failed to create test state");
        return;
    }
    
    const char *source = "var const a: int;";  // 未初始化
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_uninitialized.zr");
    TZrBool hasError = ZR_FALSE;
    SZrFunction *func = compile_source_and_check_error(state, source, strlen(source), sourceName, &hasError);
    
    if (!hasError || func != ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer,
                         "Const Local Variable Uninitialized Error",
                         "Expected compilation error but compilation succeeded");
        destroy_test_state(state);
        return;
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Local Variable Uninitialized Error");
    destroy_test_state(state);
}

// 测试 16: constructor 可以初始化声明在后面的 const 字段
static void test_const_field_declared_after_constructor_initializes_successfully(void) {
    TEST_START("Const Field Declared After Constructor Initializes Successfully");
    SZrTestTimer timer;
    timer.startTime = clock();
    
    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer,
                         "Const Field Declared After Constructor Initializes Successfully",
                         "Failed to create test state");
        return;
    }
    
    const char *source = 
        "class MyClass {\n"
        "    pub @constructor() {\n"
        "        this.id = 1;\n"
        "    }\n"
        "    pub const id: int;\n"
        "}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_field_decl_order.zr");
    SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    
    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer,
                         "Const Field Declared After Constructor Initializes Successfully",
                         "Failed to compile constructor initialization for const field declared later");
        destroy_test_state(state);
        return;
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Field Declared After Constructor Initializes Successfully");
    destroy_test_state(state);
}

// 测试 17: const 成员字段未在构造函数中初始化（应报错）
static void test_const_field_missing_constructor_initialization_error(void) {
    TEST_START("Const Field Missing Constructor Initialization Error");
    SZrTestTimer timer;
    timer.startTime = clock();
    
    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer,
                         "Const Field Missing Constructor Initialization Error",
                         "Failed to create test state");
        return;
    }
    
    const char *source = 
        "class MyClass {\n"
        "    pub const id: int;\n"
        "    pub @constructor() {\n"
        "    }\n"
        "}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_field_uninit.zr");
    TZrBool hasError = ZR_FALSE;
    SZrFunction *func = compile_source_and_check_error(state, source, strlen(source), sourceName, &hasError);
    
    if (!hasError || func != ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer,
                         "Const Field Missing Constructor Initialization Error",
                         "Expected compilation error but compilation succeeded");
        destroy_test_state(state);
        return;
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Field Missing Constructor Initialization Error");
    destroy_test_state(state);
}

// 测试 18: const 字段不允许在构造函数中使用复合赋值
static void test_const_field_compound_assignment_in_constructor_error(void) {
    TEST_START("Const Field Compound Assignment in Constructor Error");
    SZrTestTimer timer;
    timer.startTime = clock();
    
    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer,
                         "Const Field Compound Assignment in Constructor Error",
                         "Failed to create test state");
        return;
    }
    
    const char *source =
        "class MyClass {\n"
        "    pub const id: int;\n"
        "    pub @constructor() {\n"
        "        this.id += 1;\n"
        "    }\n"
        "}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_field_compound.zr");
    TZrBool hasError = ZR_FALSE;
    SZrFunction *func = compile_source_and_check_error(state, source, strlen(source), sourceName, &hasError);
    
    if (!hasError || func != ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer,
                         "Const Field Compound Assignment in Constructor Error",
                         "Expected compilation error but compilation succeeded");
        destroy_test_state(state);
        return;
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Field Compound Assignment in Constructor Error");
    destroy_test_state(state);
}

// 测试 19: if/else 两个分支各初始化一次 const 字段（应成功）
static void test_const_field_initialized_once_per_branch_success(void) {
    TEST_START("Const Field Initialized Once Per Branch Success");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer,
                         "Const Field Initialized Once Per Branch Success",
                         "Failed to create test state");
        return;
    }

    const char *source =
        "class MyClass {\n"
        "    pub const id: int;\n"
        "    pub @constructor(flag: int) {\n"
        "        if (flag == 1) {\n"
        "            this.id = 1;\n"
        "        } else {\n"
        "            this.id = 2;\n"
        "        }\n"
        "    }\n"
        "}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_field_branch_success.zr");
    SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer,
                         "Const Field Initialized Once Per Branch Success",
                         "Failed to compile constructor with one const-field initialization per branch");
        destroy_test_state(state);
        return;
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Field Initialized Once Per Branch Success");
    destroy_test_state(state);
}

// 测试 20: if 单臂分支初始化 const 字段，缺少 else（应报错）
static void test_const_field_missing_branch_initialization_error(void) {
    TEST_START("Const Field Missing Branch Initialization Error");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer,
                         "Const Field Missing Branch Initialization Error",
                         "Failed to create test state");
        return;
    }

    const char *source =
        "class MyClass {\n"
        "    pub const id: int;\n"
        "    pub @constructor(flag: int) {\n"
        "        if (flag == 1) {\n"
        "            this.id = 1;\n"
        "        }\n"
        "    }\n"
        "}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_field_branch_missing.zr");
    TZrBool hasError = ZR_FALSE;
    SZrFunction *func = compile_source_and_check_error(state, source, strlen(source), sourceName, &hasError);

    if (!hasError || func != ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer,
                         "Const Field Missing Branch Initialization Error",
                         "Expected compilation error but compilation succeeded");
        destroy_test_state(state);
        return;
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Field Missing Branch Initialization Error");
    destroy_test_state(state);
}

// 测试 21: 提前 return 导致 const 字段未在所有返回路径初始化（应报错）
static void test_const_field_early_return_before_initialization_error(void) {
    TEST_START("Const Field Early Return Before Initialization Error");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer,
                         "Const Field Early Return Before Initialization Error",
                         "Failed to create test state");
        return;
    }

    const char *source =
        "class MyClass {\n"
        "    pub const id: int;\n"
        "    pub @constructor(flag: int) {\n"
        "        if (flag == 1) {\n"
        "            return;\n"
        "        }\n"
        "        this.id = 1;\n"
        "    }\n"
        "}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_field_early_return.zr");
    TZrBool hasError = ZR_FALSE;
    SZrFunction *func = compile_source_and_check_error(state, source, strlen(source), sourceName, &hasError);

    if (!hasError || func != ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer,
                         "Const Field Early Return Before Initialization Error",
                         "Expected compilation error but compilation succeeded");
        destroy_test_state(state);
        return;
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Field Early Return Before Initialization Error");
    destroy_test_state(state);
}

// 测试 22: struct 的 if/else 两个分支各初始化一次 const 字段（应成功）
static void test_const_struct_field_initialized_once_per_branch_success(void) {
    TEST_START("Const Struct Field Initialized Once Per Branch Success");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer,
                         "Const Struct Field Initialized Once Per Branch Success",
                         "Failed to create test state");
        return;
    }

    const char *source =
        "struct MyStruct {\n"
        "    pub const id: int;\n"
        "    pub @constructor(flag: int) {\n"
        "        if (flag == 1) {\n"
        "            this.id = 1;\n"
        "        } else {\n"
        "            this.id = 2;\n"
        "        }\n"
        "    }\n"
        "}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_struct_field_branch_success.zr");
    SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer,
                         "Const Struct Field Initialized Once Per Branch Success",
                         "Failed to compile struct constructor with one const-field initialization per branch");
        destroy_test_state(state);
        return;
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Struct Field Initialized Once Per Branch Success");
    destroy_test_state(state);
}

// 测试 23: else-if 链的每条继续路径都初始化 const 字段（应成功）
static void test_const_field_initialized_once_across_else_if_chain_success(void) {
    TEST_START("Const Field Initialized Once Across Else-If Chain Success");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer,
                         "Const Field Initialized Once Across Else-If Chain Success",
                         "Failed to create test state");
        return;
    }

    const char *source =
        "class MyClass {\n"
        "    pub const id: int;\n"
        "    pub @constructor(flag: int) {\n"
        "        if (flag == 1) {\n"
        "            this.id = 1;\n"
        "        } else if (flag == 2) {\n"
        "            this.id = 2;\n"
        "        } else {\n"
        "            this.id = 3;\n"
        "        }\n"
        "    }\n"
        "}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_field_else_if_success.zr");
    SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer,
                         "Const Field Initialized Once Across Else-If Chain Success",
                         "Failed to compile constructor with full else-if initialization coverage");
        destroy_test_state(state);
        return;
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Field Initialized Once Across Else-If Chain Success");
    destroy_test_state(state);
}

// 测试 24: switch 所有 case/default 路径各初始化一次 const 字段（应成功）
static void test_const_field_switch_paths_initialized_once_success(void) {
    TEST_START("Const Field Switch Paths Initialized Once Success");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer,
                         "Const Field Switch Paths Initialized Once Success",
                         "Failed to create test state");
        return;
    }

    const char *source =
        "class MyClass {\n"
        "    pub const id: int;\n"
        "    pub @constructor(flag: int) {\n"
        "        switch (flag) {\n"
        "            (1) { this.id = 1; }\n"
        "            (2) { this.id = 2; }\n"
        "            () { this.id = 3; }\n"
        "        }\n"
        "    }\n"
        "}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_field_switch_success.zr");
    SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer,
                         "Const Field Switch Paths Initialized Once Success",
                         "Failed to compile constructor with switch-based const initialization");
        destroy_test_state(state);
        return;
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Field Switch Paths Initialized Once Success");
    destroy_test_state(state);
}

// 测试 25: switch 缺少 default 时 const 字段不能视为所有路径已初始化（应报错）
static void test_const_field_switch_missing_default_initialization_error(void) {
    TEST_START("Const Field Switch Missing Default Initialization Error");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer,
                         "Const Field Switch Missing Default Initialization Error",
                         "Failed to create test state");
        return;
    }

    const char *source =
        "class MyClass {\n"
        "    pub const id: int;\n"
        "    pub @constructor(flag: int) {\n"
        "        switch (flag) {\n"
        "            (1) { this.id = 1; }\n"
        "            (2) { this.id = 2; }\n"
        "        }\n"
        "    }\n"
        "}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_field_switch_missing_default.zr");
    TZrBool hasError = ZR_FALSE;
    SZrFunction *func = compile_source_and_check_error(state, source, strlen(source), sourceName, &hasError);

    if (!hasError || func != ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer,
                         "Const Field Switch Missing Default Initialization Error",
                         "Expected compilation error but compilation succeeded");
        destroy_test_state(state);
        return;
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Field Switch Missing Default Initialization Error");
    destroy_test_state(state);
}

// 测试 26: ternary 两个分支各初始化一次 const 字段（应成功）
static void test_const_field_ternary_branches_initialized_once_success(void) {
    TEST_START("Const Field Ternary Branches Initialized Once Success");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer,
                         "Const Field Ternary Branches Initialized Once Success",
                         "Failed to create test state");
        return;
    }

    const char *source =
        "class MyClass {\n"
        "    pub const id: int;\n"
        "    pub @constructor(flag: int) {\n"
        "        flag == 1 ? (this.id = 1) : (this.id = 2);\n"
        "    }\n"
        "}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_field_ternary_success.zr");
    SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer,
                         "Const Field Ternary Branches Initialized Once Success",
                         "Failed to compile constructor with ternary-based const initialization");
        destroy_test_state(state);
        return;
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Field Ternary Branches Initialized Once Success");
    destroy_test_state(state);
}

// 测试 27: ternary 只有一个分支初始化 const 字段（应报错）
static void test_const_field_ternary_missing_branch_initialization_error(void) {
    TEST_START("Const Field Ternary Missing Branch Initialization Error");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer,
                         "Const Field Ternary Missing Branch Initialization Error",
                         "Failed to create test state");
        return;
    }

    const char *source =
        "class MyClass {\n"
        "    pub const id: int;\n"
        "    pub @constructor(flag: int) {\n"
        "        flag == 1 ? (this.id = 1) : 2;\n"
        "    }\n"
        "}";
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, "test_const_field_ternary_missing_branch.zr");
    TZrBool hasError = ZR_FALSE;
    SZrFunction *func = compile_source_and_check_error(state, source, strlen(source), sourceName, &hasError);

    if (!hasError || func != ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer,
                         "Const Field Ternary Missing Branch Initialization Error",
                         "Expected compilation error but compilation succeeded");
        destroy_test_state(state);
        return;
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Const Field Ternary Missing Branch Initialization Error");
    destroy_test_state(state);
}

// 主测试函数
int main(void) {
    UNITY_BEGIN();
    
    TEST_MODULE_DIVIDER();
    printf("Const Keyword Tests\n");
    TEST_MODULE_DIVIDER();
    
    RUN_TEST(test_const_local_variable_declaration);
    TEST_DIVIDER();
    
    RUN_TEST(test_const_local_variable_reassignment_error);
    TEST_DIVIDER();
    
    RUN_TEST(test_const_class_field_in_constructor);
    TEST_DIVIDER();
    
    RUN_TEST(test_const_class_field_in_method_error);
    TEST_DIVIDER();
    
    RUN_TEST(test_const_struct_field_in_constructor);
    TEST_DIVIDER();
    
    RUN_TEST(test_const_struct_field_in_method_error);
    TEST_DIVIDER();
    
    RUN_TEST(test_const_interface_field_declaration);
    TEST_DIVIDER();
    
    RUN_TEST(test_const_interface_implementation_match);
    TEST_DIVIDER();
    
    RUN_TEST(test_const_function_parameter);
    TEST_DIVIDER();
    
    RUN_TEST(test_const_parameter_modification_error);
    TEST_DIVIDER();
    
    RUN_TEST(test_const_static_field_declaration);
    TEST_DIVIDER();
    
    RUN_TEST(test_const_static_field_reassignment_error);
    TEST_DIVIDER();
    
    RUN_TEST(test_const_interface_implementation_mismatch_error);
    TEST_DIVIDER();
    
    RUN_TEST(test_const_field_multiple_assignment_in_constructor);
    TEST_DIVIDER();
    
    RUN_TEST(test_const_local_variable_uninitialized_error);
    TEST_DIVIDER();
    
    RUN_TEST(test_const_field_declared_after_constructor_initializes_successfully);
    TEST_DIVIDER();

    RUN_TEST(test_const_field_missing_constructor_initialization_error);
    TEST_DIVIDER();

    RUN_TEST(test_const_field_compound_assignment_in_constructor_error);
    TEST_DIVIDER();

    RUN_TEST(test_const_field_initialized_once_per_branch_success);
    TEST_DIVIDER();

    RUN_TEST(test_const_field_missing_branch_initialization_error);
    TEST_DIVIDER();

    RUN_TEST(test_const_field_early_return_before_initialization_error);
    TEST_DIVIDER();

    RUN_TEST(test_const_struct_field_initialized_once_per_branch_success);
    TEST_DIVIDER();

    RUN_TEST(test_const_field_initialized_once_across_else_if_chain_success);
    TEST_DIVIDER();

    RUN_TEST(test_const_field_switch_paths_initialized_once_success);
    TEST_DIVIDER();

    RUN_TEST(test_const_field_switch_missing_default_initialization_error);
    TEST_DIVIDER();

    RUN_TEST(test_const_field_ternary_branches_initialized_once_success);
    TEST_DIVIDER();

    RUN_TEST(test_const_field_ternary_missing_branch_initialization_error);
    TEST_DIVIDER();
    
    return UNITY_END();
}
