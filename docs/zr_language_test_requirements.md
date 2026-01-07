# zr 语言单元测试需求文档

本文档列出了 zr 语言需要添加的单元测试，以确保语法规范的完整性和正确性。

## 1. 词法分析器测试

### 1.1 字符字面量测试

**优先级**: 高  
**状态**: 待实现

需要测试以下场景：

```zr
// 基本字符
var c1 = 'a';
var c2 = 'Z';
var c3 = '0';

// 转义字符
var c4 = '\n';      // 换行
var c5 = '\t';      // 制表符
var c6 = '\r';      // 回车
var c7 = '\b';      // 退格
var c8 = '\f';      // 换页
var c9 = '\'';      // 单引号
var c10 = '\\';     // 反斜杠

// 十六进制转义
var c11 = '\x21';   // !
var c12 = '\xFF';   // 255

// Unicode 转义
var c13 = '\u0041'; // A
var c14 = '\u4E2D'; // 中
```

**测试文件**: `tests/parser/test_char_literals.zr`

### 1.2 字符串转义序列测试

**优先级**: 中  
**状态**: 部分实现

需要确保所有转义序列正确解析：

```zr
var s1 = "hello\nworld";
var s2 = "tab\there";
var s3 = "quote\"here";
var s4 = "single\'quote";
var s5 = "backslash\\here";
var s6 = "hex\x21";
var s7 = "unicode\u4E2D";
```

**测试文件**: `tests/parser/test_string_escapes.zr`

## 2. 类型转换测试

### 2.1 基本类型转换

**优先级**: 高  
**状态**: 待实现

```zr
var i: int = <int> 3.14;
var f: float = <float> 42;
var s: string = <string> 123;
var b: bool = <bool> 1;
```

**测试文件**: `tests/parser/test_type_cast_basic.zr`

### 2.2 结构体类型转换

**优先级**: 高  
**状态**: 待实现

```zr
module "math";
struct Vector3 {
    pub var x: float;
    pub var y: float;
    pub var z: float;
}

var m = zr.import("math");
var obj = {x: 1.0, y: 2.0, z: 3.0};
var v = <m.Vector3> obj;
```

**测试文件**: `tests/parser/test_type_cast_struct.zr`

### 2.3 类类型转换

**优先级**: 高  
**状态**: 待实现

```zr
module "PersonInfo";
class Person {
    pub var id: string;
}

var k = zr.import("PersonInfo");
var p = {id: "123"};
var person = <k.Person> p;
```

**测试文件**: `tests/parser/test_type_cast_class.zr`

## 3. 泛型测试

### 3.1 静态泛型测试

**优先级**: 中  
**状态**: 待实现

```zr
class Container<T> {
    pub var value: T;
    
    pub @constructor(v: T) {
        this.value = v;
    }
}

var c1: Container<int> = new Container<int>(42);
var c2: Container<string> = new Container<string>("hello");
```

**测试文件**: `tests/parser/test_generic_static.zr`

### 3.2 动态泛型测试

**优先级**: 中  
**状态**: 待实现

```zr
class Container<T> {
    pub var value: T;
}

var c = new Container();  // T 在运行时确定
c.value = 42;
```

**测试文件**: `tests/parser/test_generic_dynamic.zr`

### 3.3 泛型约束测试

**优先级**: 中  
**状态**: 待实现

```zr
class Base {}
class Derived: Base {}

class Container<T: Base> {
    pub var value: T;
}

var c1: Container<Base> = new Container<Base>();
var c2: Container<Derived> = new Container<Derived>();
// var c3: Container<int> = new Container<int>();  // 应该报错
```

**测试文件**: `tests/parser/test_generic_constraints.zr`

## 4. 装饰器测试

### 4.1 类装饰器测试

**优先级**: 低  
**状态**: 部分实现

```zr
#singleton#
#serializable#
#custom("arg1", "arg2")#
class MyClass {
    pub var value: int;
}
```

**测试文件**: `tests/parser/test_decorator_class.zr`

### 4.2 属性装饰器测试

**优先级**: 低  
**状态**: 部分实现

```zr
class MyClass {
    #serializable#
    #editor.input#
    pub var field: int;
}
```

**测试文件**: `tests/parser/test_decorator_property.zr`

### 4.3 方法装饰器测试

**优先级**: 低  
**状态**: 部分实现

```zr
class MyClass {
    #networked#
    #cache#
    pub method(): int {
        return 1;
    }
}
```

**测试文件**: `tests/parser/test_decorator_method.zr`

## 5. 中间代码测试

### 5.1 基本中间代码测试

**优先级**: 中  
**状态**: 部分实现

```zr
intermediate TestBasic(): int %
<
>
[
    num = 1;
]
(
    var1: int
)
{
    GetConstant num;
    SetStack var1;
    FunctionReturn 0 1;
}
```

**测试文件**: `tests/parser/test_intermediate_basic.zr`

### 5.2 闭包参数测试

**优先级**: 中  
**状态**: 待实现

```zr
var outer = 10;
intermediate TestClosure(): int %
<
    outer: int
>
[
]
(
    var1: int
)
{
    GetClosure outer;
    SetStack var1;
    FunctionReturn 0 1;
}
```

**测试文件**: `tests/parser/test_intermediate_closure.zr`

## 6. 控制流表达式测试

### 6.1 if 表达式测试

**优先级**: 中  
**状态**: 部分实现

```zr
var result = if (true) {
    out 1;
} else {
    out 2;
};
```

**测试文件**: `tests/parser/test_if_expression.zr`

### 6.2 switch 表达式测试

**优先级**: 中  
**状态**: 部分实现

```zr
var result = switch (value) {
    (1) { 1; }
    (2) { 2; }
    () { 3; }
};
```

**测试文件**: `tests/parser/test_switch_expression.zr`

### 6.3 迭代器表达式测试

**优先级**: 中  
**状态**: 部分实现

```zr
var j = for (var i in [1, 2, 3, 4, 5]) {
    if (i % 2 == 0) {
        continue i;
    }
    if (i >= 4) {
        break i;
    }
};

var k = while(true) {
    if(l > 4) { break l; }
    if(l % 2 == 0) { continue l; }
};
```

**测试文件**: `tests/parser/test_iterator_expression.zr`

### 6.4 生成器表达式测试

**优先级**: 中  
**状态**: 部分实现

```zr
var k = {{
    if (Object.type(j) == "array") {
        out j.toArray();
    }
    out null;
}};
```

**测试文件**: `tests/parser/test_generator_expression.zr`

## 7. 解构赋值测试

### 7.1 对象解构测试

**优先级**: 中  
**状态**: 部分实现

```zr
var obj = {a: 1, b: 2, c: 3};
var {a, b, c} = obj;
var {a, b} = obj;  // 部分解构
```

**测试文件**: `tests/parser/test_destructure_object.zr`

### 7.2 数组解构测试

**优先级**: 中  
**状态**: 部分实现

```zr
var arr = [1, 2, 3];
var [x, y, z] = arr;
var [x] = arr;  // 部分解构
```

**测试文件**: `tests/parser/test_destructure_array.zr`

## 8. 接口测试

### 8.1 接口继承测试

**优先级**: 低  
**状态**: 部分实现

```zr
interface A {
    pub method1(): int;
}

interface B: A {
    pub method2(): int;
}
```

**测试文件**: `tests/parser/test_interface_inherit.zr`

### 8.2 接口属性访问器测试

**优先级**: 低  
**状态**: 部分实现

```zr
interface I {
    pub get set value: int;
}
```

**测试文件**: `tests/parser/test_interface_property.zr`

## 9. 枚举测试

### 9.1 枚举基类型测试

**优先级**: 低  
**状态**: 部分实现

```zr
enum E1: int {
    A = 0;
    B = 1;
}

enum E2: string {
    A = "A";
    B = "B";
}

enum E3: float {
    A = 1.0;
    B = 2.0;
}

enum E4: bool {
    A = true;
    B = false;
}
```

**测试文件**: `tests/parser/test_enum_base_types.zr`

## 10. 错误处理测试

### 10.1 语法错误测试

**优先级**: 高  
**状态**: 待实现

需要测试各种语法错误场景：

```zr
// 未闭合的字符串
var s = "hello;

// 未闭合的字符
var c = 'a;

// 无效的转义序列
var s = "\q";

// 类型不匹配
var i: int = "string";
```

**测试文件**: `tests/parser/test_syntax_errors.zr`

### 10.2 类型错误测试

**优先级**: 中  
**状态**: 待实现

```zr
// 泛型约束违反
class C {}
class B<T: C> {}
var b: B<int> = new B<int>();  // 应该报错

// 访问修饰符错误
interface I {
    pri var x: int;  // 应该报错，接口不允许 pri
}
```

**测试文件**: `tests/parser/test_type_errors.zr`

## 11. 性能测试

### 11.1 大型文件解析测试

**优先级**: 低  
**状态**: 待实现

测试解析大型 zr 文件（1000+ 行）的性能。

**测试文件**: `tests/parser/test_large_file.zr`

### 11.2 深度嵌套测试

**优先级**: 低  
**状态**: 待实现

测试深度嵌套的表达式和语句。

```zr
var result = if (a) {
    if (b) {
        if (c) {
            // ... 深度嵌套
        }
    }
};
```

**测试文件**: `tests/parser/test_deep_nesting.zr`

## 测试实现优先级

### 高优先级（必须实现）
1. 字符字面量测试
2. 类型转换测试（基本类型、结构体、类）
3. 语法错误测试

### 中优先级（应该实现）
1. 泛型测试（静态、动态、约束）
2. 控制流表达式测试
3. 解构赋值测试
4. 中间代码测试

### 低优先级（可选实现）
1. 装饰器测试
2. 接口测试
3. 枚举测试
4. 性能测试

## 测试文件组织

建议在 `tests/parser/` 目录下创建以下子目录：

```
tests/parser/
├── literals/          # 字面量测试
│   ├── test_char_literals.zr
│   └── test_string_escapes.zr
├── type_cast/         # 类型转换测试
│   ├── test_type_cast_basic.zr
│   ├── test_type_cast_struct.zr
│   └── test_type_cast_class.zr
├── generics/          # 泛型测试
│   ├── test_generic_static.zr
│   ├── test_generic_dynamic.zr
│   └── test_generic_constraints.zr
├── expressions/       # 表达式测试
│   ├── test_if_expression.zr
│   ├── test_switch_expression.zr
│   ├── test_iterator_expression.zr
│   └── test_generator_expression.zr
├── destructuring/     # 解构赋值测试
│   ├── test_destructure_object.zr
│   └── test_destructure_array.zr
├── errors/            # 错误处理测试
│   ├── test_syntax_errors.zr
│   └── test_type_errors.zr
└── performance/       # 性能测试
    ├── test_large_file.zr
    └── test_deep_nesting.zr
```

## 测试运行

每个测试文件应该包含 `%test("test_name")` 声明，测试框架应该能够：

1. 解析测试文件
2. 执行测试函数
3. 验证结果
4. 报告错误

---

**文档版本**: 1.0  
**最后更新**: 2025-01-XX

