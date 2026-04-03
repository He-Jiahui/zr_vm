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
%module "math";
struct Vector3 {
    pub var x: float;
    pub var y: float;
    pub var z: float;
}

var m = %import("math");
var obj = {x: 1.0, y: 2.0, z: 3.0};
var v = <m.Vector3> obj;
```

**测试文件**: `tests/parser/test_type_cast_struct.zr`

### 2.3 类类型转换

**优先级**: 高  
**状态**: 待实现

```zr
%module "PersonInfo";
class Person {
    pub var id: string;
}

var k = %import("PersonInfo");
var p = {id: "123"};
var person = <k.Person> p;
```

**测试文件**: `tests/parser/test_type_cast_class.zr`

## 3. 泛型测试

### 3.1 开放定义与闭型实例测试

**优先级**: 高  
**状态**: 已实现并持续扩展

```zr
class Box<T> { pub var value: T; }
struct Pair<TLeft, TRight> { pub left: TLeft; pub right: TRight; }
class Matrix<T, const N: int> { pub rows: Array<T>[N]; }

new Box<int>();
$Pair<int, string>();
new Matrix<int, 2 + 2>();
```

**覆盖点**:
- 源码 generic class / struct / interface 注册为 open generic definition
- 闭型 canonical name 与 prototype 缓存
- 嵌套泛型与 const 泛型归一化（`2 + 2 == 4`）

**测试文件**: `tests/parser/test_type_inference.c`

### 3.2 泛型函数 / 方法与类型推断测试

**优先级**: 高  
**状态**: 已实现并持续扩展

```zr
func identity<T>(value: T): T { return value; }

class Box<T> {
    func echo<U>(input: U): U { return input; }
    func shape<const N: int>(value: Matrix<T, N>): Matrix<T, N> { return value; }
}

identity(1);
identity<string>("hello");
new Box<int>().echo("hello");
new Box<int>().shape<2 + 2>(new Matrix<int, 4>());
```

**覆盖点**:
- 显式类型实参与纯推断调用
- 方法级泛型与接收者闭型联动
- const 泛型方法的显式实参与推断闭环
- 泛型参数个数错误、无法推断等专用诊断

**测试文件**: `tests/parser/test_type_inference.c`

### 3.3 约束、继承与方差测试

**优先级**: 高  
**状态**: 已实现并持续扩展

```zr
interface IProducer<out T> { next(): T; }
interface IConsumer<in T> { accept(value: T): void; }

class Base<T> { pub var value: T; }
class Derived<T> : Base<T> where T: class, new() { }
```

**覆盖点**:
- `where` 约束：基类 / 接口 / `class` / `struct` / `new()`
- 泛型继承闭包与基类成员替换
- interface `in/out` 方差保留与非法位置诊断
- 嵌套泛型中的方差位置组合

**测试文件**:
- `tests/parser/test_type_inference.c`
- `tests/language_server/test_semantic_analyzer.c`

### 3.4 参数模式与 definite assignment 测试

**优先级**: 高  
**状态**: 已实现并持续扩展

```zr
func readOnly<T>(%in value: T): void { }
func tryGet<T>(key: string, %out value: T): bool { value = null; return true; }
func swap<T>(%ref left: T, %ref right: T): void { }
```

**覆盖点**:
- `%in` 只读限制
- `%out` / `%ref` 实参必须为可赋值左值
- `%out` definite assignment
- native / source / generic call 路径的 passing mode 一致性

**测试文件**: `tests/parser/test_type_inference.c`

### 3.5 LSP 泛型签名展示测试

**优先级**: 中  
**状态**: 已实现并持续扩展

```zr
interface Producer<out T> { next(): T; }
class Derived<T, const N: int> : Producer<T> where T: class, new() { }
func swap<T>(%ref value: T): T { return value; }
```

**覆盖点**:
- hover / completion 展示 generic declaration
- const 泛型参数、`where` 约束、继承列表、方差与 `%in/%out/%ref`
- open definition 与 declaration signature 区分

**测试文件**: `tests/language_server/test_semantic_analyzer.c`

### 3.6 Native 容器 / Fixed Array 深度测试

**优先级**: 高  
**状态**: 已实现首轮深度矩阵，后续按回归继续扩展

```zr
var container = %import("zr.container");

var fixed: int[4] = [1, 2, 3, 4];
var xs: Array<int> = new container.Array<int>();
var map: Map<string, int> = new container.Map<string, int>();
var set: Set<Pair<int, string>> = new container.Set<Pair<int, string>>();
var list: LinkedList<int> = new container.LinkedList<int>();
```

**默认必跑覆盖层**:
- 运行时语义: `tests/container/test_container_runtime.c`
  - fixed array 读写、迭代、长度约束
  - `Array<T>` 扩容、插入/删除、clear、越界、结构相等
  - `Map<K,V>` 覆盖写、`[]`、Pair key、顺序无关迭代
  - `Set<T>` 唯一性、Pair 值语义
  - `Pair<K,V>` `equals` / `compareTo` / `hashCode`
  - `LinkedList<T>` 节点脱链、空移除、typed function boundary 上的 `removeFirst`
- 元数据 / 反射: `tests/container/test_container_metadata.c`
  - `zr.container` 导出项
  - open generic 参数 / 约束 / implemented interfaces
  - closed native instance canonical name、字段 / 方法 / meta-method 签名替换
- 编译 / 类型推断: `tests/container/test_container_type_inference.c`
  - fixed array 类型身份与 `ArrayLike` / `Iterable` 适配
  - native generic 约束接受 / 拒绝路径
  - `GET_MEMBER` / `SET_MEMBER` 与 `GET_BY_INDEX` / `SET_BY_INDEX` 分流后的返回类型
  - typed function return 上的 native container method 闭型保持
  - `Set` / `LinkedList` 非法 `[]` 拒绝路径
- LSP / 语义展示:
  - `tests/language_server/test_semantic_analyzer.c`
  - `tests/language_server/test_lsp_project_features.c`
  - 覆盖 native generic hover / completion、`zr.container` 模块成员补全、`LinkedNode<int>` / `Array<int>` 关闭类型展示
- 多文件集成工程:
  - `tests/fixtures/projects/container_matrix/`
  - `tests/projects/CMakeLists.txt`
  - 要求输出 deterministic banner + checksum（当前标准: `CONTAINER_MATRIX_PASS` + `635`）

**持续回归优先补充点**:
- `Map` / `Set` 迭代顺序无关聚合，不锁顺序只锁总和/集合语义
- `Map` key 与成员名冲突、`Pair` 相等 key 覆盖不增计数
- `LinkedList.clear` 后旧节点完全脱链
- fixed array 作为参数传给 `ArrayLike<T>` / `Iterable<T>` 约束目标
- 同一 closed native generic 在重复 materialize / import alias 场景下保持稳定身份

### 3.7 基础访问语义 / 迭代 / 协议 / 构造目标测试

**优先级**: 最高  
**状态**: 进行中，属于破坏式底层迁移，旧 `.zri/.zro` 产物必须重生成

```zr
var obj = { name: 1 };
var a = obj.name;
var b = obj["name"];
obj.name = 2;
obj["name"] = 3;

for (var item in values) {
    total = total + item;
}
```

**默认必跑覆盖层**:
- parser / lowering: `tests/parser/test_compiler_features.c`
  - `.` 只能落成 `GET_MEMBER` / `SET_MEMBER`
  - `[]` 只能落成 `GET_BY_INDEX` / `SET_BY_INDEX`
  - emitted `.zri` 不允许再出现 `GETTABLE` / `SETTABLE` / `DYN_GET` / `DYN_SET`
- SemIR / ExecBC / AOT artifact: `tests/parser/test_semir_pipeline.c` + `tests/parser/test_execbc_aot_pipeline.c` + `tests/parser/test_dynamic_iteration_pipeline.c`
  - SemIR enum 与文本产物不再保留 `DYN_GET` / `DYN_SET`
  - AOT C / LLVM artifact 不再声明仅服务旧动态访问 quickening 的 runtime contract
  - 禁止保留旧访问兼容 quickening，例如 `SUPER_GET_GLOBAL_NAMED`
  - dynamic foreach 允许在 ExecBC 上把 `DYN_ITER_MOVE_NEXT + JUMP_IF(false)` 融合成 `SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE`
  - 该 superinstruction 只允许出现在 ExecBC；`.zri` 的 SemIR 区段和 AOT C / LLVM artifact 仍必须保留 `DYN_ITER_INIT` / `DYN_ITER_MOVE_NEXT`
  - 零参数 `FUNCTION_CALL` / `DYN_CALL` / `META_CALL` / `FUNCTION_TAIL_CALL` / `DYN_TAIL_CALL` / `META_TAIL_CALL` 允许在 ExecBC 上 quicken 成对应的 `SUPER_*_CALL_NO_ARGS`
  - 这些 zero-arg superinstruction 只允许出现在 ExecBC；`SemIR` / AOT artifact 仍必须保留原始语义 opcode
- dynamic/meta call artifact: `tests/parser/test_meta_call_pipeline.c` + `tests/parser/test_tail_call_pipeline.c`
  - 非 tail `@call` 站点必须显式保留 `META_CALL`
  - tail position 上无法静态解析的 callable 必须显式保留 `DYN_TAIL_CALL`
  - tail position 上的 `@call` receiver 必须显式保留 `META_TAIL_CALL`
  - property getter/setter 必须直接落成 ExecBC `META_GET` / `META_SET`，不能再退化成 `GET_MEMBER + FUNCTION_CALL (+ SET_STACK)` helper 序列
  - `SemIR` / `.zri` / AOT C / AOT LLVM 中必须显式保留 `META_GET` / `META_SET`
  - `.zri`、AOT C、AOT LLVM 三类产物都必须保留这些 opcode 名称，而不是退化成普通 `FUNCTION_TAIL_CALL`
  - `DYN_CALL` / `DYN_TAIL_CALL` / `META_CALL` / `META_TAIL_CALL` / `META_GET` / `META_SET` 必须统一携带 shared `FUNCTION_PRECALL` runtime contract 或等价 runtime contract 与 deopt metadata
  - 深尾递归必须证明 `callInfo` 链保持有界，而不是只验证结果值正确
  - 带活动异常处理器或 `%using` cleanup 的 frame 必须允许显式回退到非复用路径，不能为了 TCO 破坏现有展开语义
- ownership lifecycle artifact: `tests/parser/test_parser.c` + `tests/parser/test_compiler_features.c` + `tests/parser/test_execbc_aot_pipeline.c`
  - parser 必须把 `%upgrade(expr)` / `%release(expr)` 解析成 dedicated ownership builtin，而不是继续复用普通 helper-call 形态
  - ExecBC 必须显式保留 `OWN_UPGRADE` / `OWN_RELEASE`
  - `SemIR`、`.zri`、AOT C、AOT LLVM 必须显式保留 `OWN_UPGRADE` / `OWN_RELEASE`
  - AOT artifact 必须声明 `ZrCore_Ownership_UpgradeValue` / `ZrCore_Ownership_ReleaseValue` runtime contract 或等价 contract
  - `%upgrade(weak)` 在仍有 shared owner 时必须返回非空 shared；最后一个 shared owner 消失后再次升级必须返回 `null`
  - `%release` 当前若尚未引入 place-aware ownership effect，至少必须限制为 local identifier binding，不能假装正确支持 member/index/closure-place release
- runtime contracts: `tests/instructions/test_instructions.c`
  - member descriptor、index contract、iterable contract、iterator contract 全部走显式 contract dispatch
  - `META_GET` / `META_SET` 必须直接调用 hidden accessor runtime path，而不是先退回普通成员取值再走 helper call
  - `SUPER_META_GET_CACHED` / `SUPER_META_SET_CACHED` 必须通过 `CALLSITE_CACHE_TABLE` 命中，并在 receiver prototype 改变时重新 guard 不能错用旧 accessor
  - static property accessor site 必须允许进一步 quicken 成 `SUPER_META_GET_STATIC_CACHED` / `SUPER_META_SET_STATIC_CACHED`
  - static hidden accessor invocation 不能额外注入 receiver
  - 禁止仅靠 `getIterator` / `moveNext` / `current` 字段名字满足 `foreach`
- compiler + runtime integration: `tests/parser/test_instruction_execution.c`
  - binary import roundtrip 后仍保留 `GET_MEMBER` metadata
  - `GET_MEMBER` / `GET_BY_INDEX` 混合链式访问、赋值、模板字符串、全局 sugar 都要回归
- native/container/module integration:
  - `tests/container/test_container_runtime.c`
  - `tests/module/test_module_system.c`
  - 覆盖 field、method、property、static member、prototype inherited member、dynamic member opt-in、array/map/plain object/custom indexable、native iterable、custom iterable
- project / CLI / artifact:
  - `tests/cmake/run_cli_suite.cmake`
  - `tests/fixtures/projects/classes/`
  - `tests/fixtures/projects/native_numeric_pipeline/`
  - 旧访问语义产生的 `.zri/.zro` 视为失效产物，fixture 需要重新编译，不保留运行时兼容分支

**持续回归优先补充点**:
- protocol conformance 统一走稳定 `protocol_id`，不再依赖 `Iterable` / `Iterator` / `ArrayLike` 字符串白名单
- construct-target resolution 统一走 prototype identity，不再按 `"Array"` / `"Map"` / `"Tensor"` / `"Pair"` 等名字复用 self
- property accessor、static member、prototype member、dynamic member write opt-in 的顺序冲突和边界错误路径
- 空迭代器、失效迭代器、GC 下活跃 iterator/constructor 对象、重复导入 `.zro` 的压力测试

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

