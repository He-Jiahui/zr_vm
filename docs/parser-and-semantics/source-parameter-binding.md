---
related_code:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_parameters.c
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_spread.c
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_rest_arguments.c
  - zr_vm_core/src/zr_vm_core/function/function.c
  - zr_vm_core/src/zr_vm_core/function/function_rest_parameters.c
  - zr_vm_core/src/zr_vm_core/function/function_precall_internal.h
  - tests/cli/source_parameter_binding.cmake
  - tests/parser/test_call_spread.c
---

# 源码函数的默认参数和数组变参

普通源码函数使用 `name: Type = expression` 声明默认参数。调用没有提供某个固定参数时，
编译器把其默认表达式放到缺少的实参槽。`f()` 没有实参名字表，仍然必须经过这一过程。
显式提供的 `null` 是实参；能否用于该参数取决于参数类型。

```zr
fn count(first: int = 7, ...items: int[]): int {
    return first + items.length;
}

count();               // 7，items 长度为 0
count(7, 10, 20);      // 9，items 为 [10, 20]
count(7, ...[10, 20]); // 9
```

`...items: T[]` 必须是最后一个参数，按值绑定到一个新的、未限定长度的数组。
`...items: T`、`...items: T[2]`、借用形式和默认表达式均不合法。固定参数仍按
原来的命名或位置规则绑定；多余的位置实参及尾部展开的剩余元素按顺序写入数组。
编译器检查尾部元素和 `T` 是否兼容，直接复制元素的调用路径不做隐式元素转换。

## 实现路径

1. `parse_parameter_list` 在 `, ...` 前停止，函数声明把变参保存在独立的 `args` 字段。
   `parse_parameter` 检查数组类型、位置和传参形式。这样变参不会混进固定参数计数。
2. 函数体把数组注册在固定参数之后的第一个本地槽位。直接调用只用固定参数参与既有
   签名匹配，同时检查变参元素，保留原来的求值顺序和实参数量。空实参仍可补入默认值。
3. VM 在进入函数之前收集尾部实参，用临时 GC 根保留数组直到它进入函数帧。
   VM 在初始化帧布局之后绑定该槽位，因此存在独立的布局存储时也会写入正确地址。
   `parameterCount` 继续指固定参数数目；变参函数使用常规参数绑定，不能走固定参数的
   快速入口或者直接复用尾调用帧。

这里描述的是普通源码函数的直接调用。函数值的静态签名、仅以变参区分的重载、原生签名
及导入签名的调用规则不在这次变更内。

## 验证

计划来源：用户提交的 `f()` 返回 `null` 复现，以及 `...params: int[]` 的语法要求。
安装包中的旧 CLI 对空实参返回 `null`，也不能在函数体读到展开后的数组。
`source_parameter_binding` 用源码 CLI 验证全部省略、显式 `null`、命名参数与默认值、
空数组、不同数量的尾部实参、尾部展开、独立数组和不合法的声明或元素类型。
`call_spread` 覆盖语法树及原有展开调用。可分别运行：

```text
ctest --test-dir <build-directory> -R '^(source_parameter_binding|call_spread)$' --output-on-failure
<build-directory>/bin/zr_vm_named_arguments_test
```
