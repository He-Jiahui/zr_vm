# ZR-VM 测试执行顺序文档

本文档描述了 ZR-VM 测试套件的执行顺序和组织结构，按照**由浅入深、由先到后、由局部到整体**的原则组织。

## 测试执行策略

### 执行原则

1. **由浅入深**：从基础组件测试开始，逐步深入到复杂功能测试
2. **由先到后**：按照依赖关系，先测试被依赖的模块，再测试依赖其他模块的测试
3. **由局部到整体**：先测试单个功能点，再测试完整流程

### 测试层级

- **Level 1（基础层）**：最底层的基础组件，不依赖其他模块
- **Level 2（分析层）**：词法和语法分析，依赖基础层
- **Level 3（编译层）**：编译和类型系统，依赖分析层
- **Level 4（执行层）**：VM执行和综合测试，依赖所有层

## 测试执行顺序

### Level 1: 基础组件测试

#### 1. GC 测试 (`zr_vm_gc_test`)
- **描述**：垃圾回收器基础功能测试
- **依赖**：无（最底层）
- **测试内容**：
  - 基础类型测试
  - GC 状态宏测试
  - GC 函数测试
  - 边界条件测试
  - GC 扫描阶段测试
  - GC 标记遍历测试
  - GC 根标记测试
  - GC 状态机测试
  - GC 分代测试
  - GC 原生数据测试
  - GC 完整收集测试
  - GC 步进测试
  - GC 屏障测试

#### 2. 指令测试 (`zr_vm_instructions_test`)
- **描述**：VM 指令执行基础测试
- **依赖**：core 模块
- **测试内容**：
  - 基本指令执行
  - 指令参数验证
  - 指令错误处理

#### 3. Meta 测试 (`zr_vm_meta_test`)
- **描述**：元方法基础功能测试
- **依赖**：core 模块
- **测试内容**：
  - 元方法定义
  - 元方法调用
  - 元方法查找

### Level 2: 词法和语法分析测试

#### 4. Lexer 测试 (`zr_vm_lexer_parser_compiler_execution_test` - Lexer 部分)
- **描述**：词法分析器测试
- **依赖**：core 模块
- **测试内容**：
  - 字符字面量 token 识别
  - 字符串字面量 token 识别
  - 字符转义序列解析
  - 关键字识别
  - 操作符识别
  - 标识符识别

#### 5. Parser 基础测试 (`zr_vm_parser_test`)
- **描述**：语法分析器基础测试
- **依赖**：core 模块、parser 模块
- **测试内容**：
  - 字面量解析（整数、浮点数、字符串、布尔值、null）
  - 变量声明解析
  - 函数声明解析
  - 结构体声明解析
  - 类声明解析
  - 控制流语句解析（if、while、for、switch）
  - 表达式解析
  - 对象字面量解析
  - 数组字面量解析
  - 完整文件解析（test_simple.zr）

#### 6. 字符字面量和类型转换测试 (`zr_vm_char_and_type_cast_test`)
- **描述**：扩展语法特性测试
- **依赖**：core 模块、parser 模块
- **测试内容**：
  - 字符字面量解析和编译
  - 基本类型转换（int、float、string、bool）
  - 结构体类型转换
  - 类类型转换

### Level 3: 编译和类型系统测试

#### 7. 类型推断测试 (`zr_vm_type_inference_test`)
- **描述**：类型推断系统测试
- **依赖**：core 模块、parser 模块
- **测试内容**：
  - 基本类型推断
  - 函数返回类型推断
  - 变量类型推断
  - 表达式类型推断

#### 8. 编译器功能测试 (`zr_vm_compiler_features_test`)
- **描述**：编译器功能测试
- **依赖**：core 模块、parser 模块
- **测试内容**：
  - AST 到指令码转换
  - 常量生成
  - 栈槽分配
  - 指令生成
  - 函数编译
  - 控制流编译

#### 9. Prototype 测试 (`zr_vm_prototype_test`)
- **描述**：类型系统高级特性测试
- **依赖**：core 模块、parser 模块
- **测试内容**：
  - 结构体 prototype 生成
  - 类 prototype 生成
  - Prototype 继承
  - Prototype 模块导出
  - Prototype 运行时创建

### Level 4: 执行和综合测试

#### 10. 指令执行测试 (`zr_vm_instruction_execution_test`)
- **描述**：VM 执行引擎测试
- **依赖**：core 模块、parser 模块
- **测试内容**：
  - 基本指令执行
  - 算术运算指令
  - 逻辑运算指令
  - 类型转换指令执行
  - 函数调用指令
  - 控制流指令

#### 11. 模块系统测试 (`zr_vm_module_system_test`)
- **描述**：模块加载和导出测试
- **依赖**：core 模块、parser 模块
- **测试内容**：
  - 模块声明
  - 模块导入
  - 模块导出（public、protected）
  - 模块缓存
  - 模块依赖解析

#### 12. 异常处理测试 (`zr_vm_exceptions_test`)
- **描述**：异常机制测试
- **依赖**：core 模块、parser 模块
- **测试内容**：
  - try-catch-finally 语句
  - 异常抛出
  - 异常捕获
  - 异常传播

#### 13. 综合脚本测试 (`zr_vm_scripts_test`)
- **描述**：完整流程综合测试
- **依赖**：所有模块
- **测试内容**：
  - 完整脚本解析
  - 完整脚本编译
  - 完整脚本执行
  - 复杂语法特性组合
  - 实际应用场景模拟

## 测试依赖关系图

```
Level 1 (基础层)
├── GC 测试 (无依赖)
├── 指令测试 (依赖 core)
└── Meta 测试 (依赖 core)

Level 2 (分析层)
├── Lexer 测试 (依赖 core)
├── Parser 基础测试 (依赖 core, parser)
└── 字符字面量和类型转换测试 (依赖 core, parser)

Level 3 (编译层)
├── 类型推断测试 (依赖 core, parser)
├── 编译器功能测试 (依赖 core, parser)
└── Prototype 测试 (依赖 core, parser)

Level 4 (执行层)
├── 指令执行测试 (依赖 core, parser)
├── 模块系统测试 (依赖 core, parser)
├── 异常处理测试 (依赖 core, parser)
└── 综合脚本测试 (依赖所有模块)
```

## 运行测试

### 运行所有测试

```bash
# 使用测试运行器（推荐）
./zr_vm_test_runner

# 或使用 CTest
ctest
```

### 运行特定测试

```bash
# 运行特定测试套件
./zr_vm_test_runner gc_tests
./zr_vm_test_runner parser_tests
./zr_vm_test_runner scripts_tests

# 查看所有可用测试
./zr_vm_test_runner --help
```

### 运行单个测试可执行文件

```bash
# Level 1
./zr_vm_gc_test
./zr_vm_instructions_test
./zr_vm_meta_test

# Level 2
./zr_vm_parser_test
./zr_vm_char_and_type_cast_test
./zr_vm_lexer_parser_compiler_execution_test

# Level 3
./zr_vm_type_inference_test
./zr_vm_compiler_features_test
./zr_vm_prototype_test

# Level 4
./zr_vm_instruction_execution_test
./zr_vm_module_system_test
./zr_vm_exceptions_test
./zr_vm_scripts_test
```

## 测试文件组织

### 测试文件结构

```
tests/
├── test_runner.c              # 主测试运行器
├── TEST_EXECUTION_ORDER.md    # 本文档
├── CMakeLists.txt            # 测试主 CMakeLists
│
├── gc/                       # GC 测试
│   ├── gc_tests.c
│   ├── gc_test_utils.c
│   └── CMakeLists.txt
│
├── parser/                   # Parser 测试
│   ├── test_parser.c
│   ├── test_lexer_parser_compiler_execution.c
│   ├── test_char_and_type_cast.c
│   ├── test_type_inference.c
│   ├── test_compiler_features.c
│   ├── test_instruction_execution.c
│   ├── test_prototype.c
│   ├── test_*.zr             # 测试用例文件
│   └── CMakeLists.txt
│
├── instructions/             # 指令测试
│   ├── test_instructions.c
│   └── CMakeLists.txt
│
├── meta/                     # Meta 测试
│   ├── test_meta.c
│   ├── meta.c
│   └── CMakeLists.txt
│
├── module/                   # 模块系统测试
│   ├── test_module_system.c
│   └── CMakeLists.txt
│
├── exceptions/               # 异常处理测试
│   ├── test_exceptions.c
│   └── CMakeLists.txt
│
└── scripts/                  # 综合脚本测试
    ├── test_comprehensive.c
    ├── test_utils.c
    ├── test_cases/           # 测试用例目录
    └── CMakeLists.txt
```

## 测试规范

所有测试都应遵循以下规范：

1. **测试开始**：输出 `Unit Test - ${Test Summary}`
2. **测试信息**：输出 `Testing ${Summary}:\n ${WhatIsTesting}`
3. **测试完成**：输出 `Pass - Cost Time:${Time} - ${Summary}` 或 `Fail - Cost Time:${Time} - ${Summary}:\n ${Reason}`
4. **测试分割**：使用 `----` 分割单个测试，使用 `==========` 分割测试模块

## 注意事项

1. **测试顺序很重要**：必须按照依赖关系顺序执行，否则可能导致测试失败
2. **测试隔离**：每个测试应该独立运行，不依赖其他测试的状态
3. **资源清理**：测试完成后必须清理所有分配的资源
4. **错误处理**：测试应该优雅地处理错误情况，并提供有意义的错误信息


