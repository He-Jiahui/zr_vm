//
// Created by Auto on 2025/01/XX.
// 主测试运行器 - 按照由浅入深、由先到后、由局部到整体的顺序执行所有测试
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "zr_vm_common/zr_common_conf.h"

// 测试执行结果
typedef struct {
    const char* testName;
    int exitCode;
    double executionTime;
    TBool passed;
} SZrTestResult;

// 测试套件信息
typedef struct {
    const char* suiteName;
    const char* description;
    const char* executable;
    int level;  // 测试层级：1=基础，2=中级，3=高级，4=综合
    int order;  // 执行顺序
} SZrTestSuite;

// 测试套件定义（按照执行顺序）
static SZrTestSuite testSuites[] = {
    // ========== 第一层：基础组件测试（Level 1）==========
    // 1. GC 基础测试（最底层，不依赖其他模块）
    {"gc_tests", "GC基础功能测试", "zr_vm_gc_test", 1, 1},
    
    // 2. 指令测试（基础指令执行，依赖 core）
    {"instructions_tests", "VM指令执行测试", "zr_vm_instructions_test", 1, 2},
    
    // 3. Meta 测试（元方法基础功能，依赖 core）
    {"meta_tests", "元方法功能测试", "zr_vm_meta_test", 1, 3},
    
    // ========== 第二层：词法和语法分析测试（Level 2）==========
    // 4. Lexer 测试（词法分析，最基础的解析功能）
    {"lexer_tests", "词法分析器测试", "zr_vm_lexer_parser_compiler_execution_test", 2, 4},
    
    // 5. Parser 基础测试（语法分析基础）
    {"parser_tests", "语法分析器基础测试", "zr_vm_parser_test", 2, 5},
    
    // 6. 字符字面量和类型转换测试（扩展语法特性）
    {"char_and_type_cast_tests", "字符字面量和类型转换测试", "zr_vm_char_and_type_cast_test", 2, 6},
    
    // ========== 第三层：编译和类型系统测试（Level 3）==========
    // 7. 类型推断测试（类型系统基础）
    {"type_inference_tests", "类型推断测试", "zr_vm_type_inference_test", 3, 7},
    
    // 8. 编译器功能测试（编译功能）
    {"compiler_features_tests", "编译器功能测试", "zr_vm_compiler_features_test", 3, 8},
    
    // 9. Prototype 测试（类型系统高级特性）
    {"prototype_tests", "Prototype生成测试", "zr_vm_prototype_test", 3, 9},
    
    // ========== 第四层：执行和综合测试（Level 4）==========
    // 10. 指令执行测试（VM执行引擎）
    {"instruction_execution_tests", "指令执行测试", "zr_vm_instruction_execution_test", 4, 10},
    
    // 11. 模块系统测试（模块加载和导出）
    {"module_tests", "模块系统测试", "zr_vm_module_system_test", 4, 11},
    
    // 12. 异常处理测试（异常机制）
    {"exceptions_tests", "异常处理测试", "zr_vm_exceptions_test", 4, 12},
    
    // 13. 综合脚本测试（完整流程）
    {"scripts_tests", "综合脚本测试", "zr_vm_scripts_test", 4, 13},
    
    {ZR_NULL, ZR_NULL, ZR_NULL, 0, 0}  // 结束标记
};

// 执行单个测试套件
static SZrTestResult run_test_suite(SZrTestSuite* suite) {
    SZrTestResult result;
    result.testName = suite->suiteName;
    result.passed = ZR_FALSE;
    
    clock_t startTime = clock();
    
    printf("\n");
    printf("==========\n");
    printf("Running: %s\n", suite->description);
    printf("Executable: %s\n", suite->executable);
    printf("Level: %d, Order: %d\n", suite->level, suite->order);
    printf("==========\n");
    fflush(stdout);
    
    // 执行测试可执行文件
    char command[512];
    char* paths[3] = {ZR_NULL, ZR_NULL, ZR_NULL};
    int exitCode = -1;
    
    // 构建可能的路径
    snprintf(command, sizeof(command), "./%s", suite->executable);
    paths[0] = command;
    
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != ZR_NULL) {
        TZrSize cwdLen = strlen(cwd);
        TZrSize exeLen = strlen(suite->executable);
        paths[1] = (char*)malloc(cwdLen + exeLen + 30);
        if (paths[1] != ZR_NULL) {
            sprintf(paths[1], "%s/%s", cwd, suite->executable);
        }
        
        paths[2] = (char*)malloc(cwdLen + exeLen + 50);
        if (paths[2] != ZR_NULL) {
            sprintf(paths[2], "%s/bin/%s", cwd, suite->executable);
        }
    }
    
    // 使用 system 执行测试
    // 首先尝试 bin 目录（最可能的位置）
    if (paths[2] != ZR_NULL) {
        exitCode = system(paths[2]);
    }
    
    // 如果失败，尝试当前目录
    if (exitCode != 0) {
        exitCode = system(command);
    }
    
    // 如果还是失败，尝试使用完整路径
    if (exitCode != 0 && paths[1] != ZR_NULL) {
        exitCode = system(paths[1]);
    }
    
    // 清理动态分配的内存
    if (paths[1] != ZR_NULL) {
        free(paths[1]);
    }
    if (paths[2] != ZR_NULL) {
        free(paths[2]);
    }
    
    clock_t endTime = clock();
    result.executionTime = ((double)(endTime - startTime) / CLOCKS_PER_SEC) * 1000.0;
    result.exitCode = exitCode;
    result.passed = (exitCode == 0);
    
    return result;
}

// 主测试运行函数
int main(int argc, char* argv[]) {
    printf("\n");
    printf("========================================\n");
    printf("ZR-VM Comprehensive Test Suite Runner\n");
    printf("========================================\n");
    printf("\n");
    printf("Test Execution Strategy:\n");
    printf("  - Level 1: Basic Components (GC, Instructions, Meta)\n");
    printf("  - Level 2: Lexical and Syntax Analysis (Lexer, Parser)\n");
    printf("  - Level 3: Compilation and Type System (Type Inference, Compiler, Prototype)\n");
    printf("  - Level 4: Execution and Integration (VM Execution, Modules, Exceptions, Scripts)\n");
    printf("\n");
    printf("========================================\n");
    printf("\n");
    
    // 检查是否指定了特定测试
    TBool runSpecificTest = ZR_FALSE;
    const char* specificTest = ZR_NULL;
    
    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("Usage: %s [test_name]\n", argv[0]);
            printf("\nAvailable tests:\n");
            for (int i = 0; testSuites[i].suiteName != ZR_NULL; i++) {
                printf("  %s - %s (Level %d, Order %d)\n", 
                       testSuites[i].suiteName, 
                       testSuites[i].description,
                       testSuites[i].level,
                       testSuites[i].order);
            }
            return 0;
        }
        runSpecificTest = ZR_TRUE;
        specificTest = argv[1];
    }
    
    // 统计信息
    int totalTests = 0;
    int passedTests = 0;
    int failedTests = 0;
    double totalTime = 0.0;
    
    // 按顺序执行测试
    for (int i = 0; testSuites[i].suiteName != ZR_NULL; i++) {
        // 如果指定了特定测试，只运行该测试
        if (runSpecificTest) {
            if (strcmp(testSuites[i].suiteName, specificTest) != 0) {
                continue;
            }
        }
        
        totalTests++;
        SZrTestResult result = run_test_suite(&testSuites[i]);
        
        if (result.passed) {
            passedTests++;
            printf("\n✓ PASSED: %s (%.3fms)\n", result.testName, result.executionTime);
        } else {
            failedTests++;
            printf("\n✗ FAILED: %s (%.3fms, exit code: %d)\n", 
                   result.testName, result.executionTime, result.exitCode);
        }
        
        totalTime += result.executionTime;
        
        printf("----------\n");
        fflush(stdout);
    }
    
    // 输出总结
    printf("\n");
    printf("========================================\n");
    printf("Test Suite Summary\n");
    printf("========================================\n");
    printf("Total Tests: %d\n", totalTests);
    printf("Passed: %d\n", passedTests);
    printf("Failed: %d\n", failedTests);
    printf("Total Time: %.3fms\n", totalTime);
    printf("========================================\n");
    printf("\n");
    
    return (failedTests == 0) ? 0 : 1;
}

