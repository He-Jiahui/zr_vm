# zr 语言完整语法规范

本文档基于 `test_simple.zr` 示例文件和解析器实现，总结了 zr 语言的完整语法规范。

## 目录

1. [词法规范](#1-词法规范)
2. [语法规范](#2-语法规范)
3. [特殊语法特性](#3-特殊语法特性)
4. [文件结构](#4-文件结构)
5. [语法规则总结](#5-语法规则总结)
6. [已知问题和待实现功能](#6-已知问题和待实现功能)

---

## 1. 词法规范

### 1.1 关键字

#### 模块和类型声明
- `module` - 模块声明
- `struct` - 结构体声明
- `class` - 类声明
- `interface` - 接口声明
- `enum` - 枚举声明
- `intermediate` - 中间代码声明

#### 变量和函数
- `var` - 变量声明
- `static` - 静态成员声明

#### 访问修饰符
- `pub` - public（公开）
- `pri` - private（私有）
- `pro` - protected（受保护）

#### 控制流
- `if` - 条件语句
- `else` - else 分支
- `switch` - 开关语句
- `while` - while 循环
- `for` - for 循环
- `break` - 跳出循环
- `continue` - 继续循环
- `return` - 返回语句

#### 异常处理
- `try` - 异常捕获开始
- `catch` - 异常捕获
- `finally` - 最终处理
- `throw` - 抛出异常

#### 特殊关键字
- `new` - 创建类实例
- `super` - 调用父类
- `in` - foreach 循环中的迭代关键字
- `out` - 生成器表达式中的输出关键字
- `get` - 属性访问器（getter）
- `set` - 属性访问器（setter）

#### 测试
- `test` - 测试声明（配合 `%` 使用）

#### 特殊值
- `Infinity` - 正无穷
- `NegativeInfinity` - 负无穷
- `NaN` - 非数字
- `true` - 布尔真值
- `false` - 布尔假值
- `null` - 空值

### 1.2 操作符

#### 算术操作符
- `+` - 加法
- `-` - 减法
- `*` - 乘法
- `/` - 除法
- `%` - 取模

#### 赋值操作符
- `=` - 赋值
- `+=` - 加法赋值
- `-=` - 减法赋值
- `*=` - 乘法赋值
- `/=` - 除法赋值
- `%=` - 取模赋值

#### 比较操作符
- `==` - 相等
- `!=` - 不等
- `<` - 小于
- `>` - 大于
- `<=` - 小于等于
- `>=` - 大于等于

#### 逻辑操作符
- `&&` - 逻辑与
- `||` - 逻辑或
- `!` - 逻辑非

#### 位运算操作符
- `&` - 位与
- `|` - 位或
- `^` - 位异或
- `~` - 位取反
- `<<` - 左移
- `>>` - 右移

#### 其他操作符
- `?` - 三元运算符
- `:` - 类型注解、三元运算符
- `=>` - 箭头函数
- `...` - 可变参数、展开操作符
- `.` - 成员访问
- `@` - 元函数标识符
- `#` - 装饰器
- `$` - 值类型构造
- `<` - 小于、泛型开始、类型转换
- `>` - 大于、泛型结束、类型转换

### 1.3 字面量

#### 整数字面量
- **十进制**: `123`, `0`, `-456`
- **十六进制**: `0xFF`, `0xABCD`, `0xffffffffffffffff`
- **八进制**: `0777`, `0123`

#### 浮点数字面量
- **双精度**: `1.0`, `3.14`, `-0.5`
- **单精度**: `1.0f`, `3.14F`
- **双精度显式**: `1.0d`, `3.14D`
- **科学计数法**: `1e10`, `1.5e-3`

#### 字符串字面量
- **双引号**: `"hello"`, `"world"`
- **单引号**: `'hello'`, `'world'`
- **转义序列**:
  - `\n` - 换行
  - `\t` - 制表符
  - `\r` - 回车
  - `\b` - 退格
  - `\f` - 换页
  - `\"` - 双引号
  - `\'` - 单引号
  - `\\` - 反斜杠
  - `\xXX` - 十六进制字符（2位）
  - `\uXXXX` - Unicode 字符（4位）

#### 字符字面量
- **格式**: `'a'`, `'\n'`, `'\x21'`, `'\''`
- **转义序列**: 与字符串相同

#### 布尔字面量
- `true` - 真
- `false` - 假

#### 空值字面量
- `null` - 空值

---

## 2. 语法规范

### 2.1 模块声明

```zr
module "module_name";
// 或
module module_name;
```

**说明**:
- 模块名可以是字符串或标识符
- 如果包含特殊符号，必须使用字符串形式
- 模块声明必须在文件开头（可选）

### 2.2 变量声明

```zr
// 基本声明
var name = value;
var name: Type = value;

// 解构赋值（对象）
var {a, b, c} = object;

// 解构赋值（数组）
var [x, y] = array;
var [x] = array;
```

**说明**:
- 类型注解可选，支持类型推断
- 支持对象和数组解构

### 2.3 函数声明

```zr
// 普通函数
functionName(param1: Type, param2: Type): ReturnType {
    // body
}

// 可变参数
functionName(...args: Type[]) {
    // body
}

// 混合参数
functionName(param1: Type, ...args: Type[]) {
    // body
}

// Lambda 表达式
var func = (param1, param2) => {
    // body
};

var func = (param1: Type, param2: Type): ReturnType => {
    // body
};
```

**说明**:
- 支持函数重载
- 支持可变参数（`...args`）
- Lambda 表达式支持类型推断

### 2.4 结构体 (struct)

```zr
struct Vector3 {
    // 字段声明
    pub var x: float;
    var y: float;
    var z: float = 0;
    var a;  // 类型推断为 object
    
    // 静态成员
    static var ZERO = $Vector3(0, 0, 0);
    
    // 构造函数
    pub @constructor() {
        this.x = 0;
    }
    
    pub @constructor(x: float, y: float, z: float) {
        this.x = x;
        this.y = y;
        this.z = z;
    }
    
    // 元方法（静态）
    pub static @add(lhs: Vector3, rhs: Vector3): Vector3 {
        return $Vector3(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z);
    }
    
    // 普通方法
    pub add(rhs: Vector3): Vector3 {
        return $Vector3(this.x + rhs.x, this.y + rhs.y, this.z + rhs.z);
    }
}
```

**特点**:
- **值类型**: 使用 `$TypeName(...)` 创建实例
- **不支持继承**: struct 不能继承其他类型
- **支持静态成员和方法**: static 成员存储在原型
- **支持元函数**: `@` 开头的特殊函数（如 `@constructor`）
- **支持访问修饰符**: pub, pri, pro

### 2.5 类 (class)

```zr
// 类装饰器
#singleton#
#serializable#
class Person {
    // 成员装饰器
    #header("Person Info")#
    #editor.input#
    
    // 字段
    pub var id: string;
    pri var _address: string;
    pro var telephone: string;
    
    // 静态成员（存储在原型）
    pri static var _address2: string;
    pro static var telephone2: string;
    
    // 构造函数
    pub @constructor(id: string) {
    }
    
    // 属性访问器
    #editor.input#
    pub set address(val: string) {
        this._address = val;
    }
    
    pub get address {
        return this._address;
    }
    
    pub static get address2 {
        // 此时 this 为类原型
        return this._address2;
    }
}

// 继承和接口实现
class Student: Person, Man {
    pub @constructor(id: string) super(id) {
        // body
    }
}
```

**特点**:
- **引用类型**: 使用 `new` 创建实例
- **支持单继承和多个接口实现**: `class Child: Parent, Interface1, Interface2`
- **支持装饰器**: `#...#` 语法
- **支持属性访问器**: `get`/`set`
- **支持静态成员**: static 成员存储在原型
- **支持访问修饰符**: pub, pri, pro

### 2.6 接口 (interface)

```zr
interface Man: arr {
    pub var id: string;
    pub var telephone;
    pro var _address: string;
    
    // 访问器（接口中只能声明，不能实现）
    pub get set address: int;
    
    // 方法签名
    pub @constructor(id: string);
    pub getName(): string;
}
```

**特点**:
- **只能声明方法和成员，不实现**: 接口只定义契约
- **访问性检查**: 只允许 `pub` 和 `pro`（不允许 `pri`）默认使用`pub`
- **可以继承其他类型**: `interface A: B, C`
- **访问器可以声明不同的 getter 和 setter 类型**

### 2.7 枚举 (enum)

```zr
// 默认 int 类型，从 0 开始递增
enum Dimension {
    x;
    y;
    z;
}

// 显式指定基类型和值
enum RGBA: string {
    R = "R",
    G = "G",
    B = "B",
    A = "A",
}

enum Status: int {
    Pending = 0;
    Running = 1;
    Completed = 2;
}
```

**特点**:
- **可以继承基类型**: `int`, `string`, `float`, `bool`
- **成员可以显式赋值或使用默认值**: 默认从 0 开始递增
- **成员分隔符**: 可以使用 `,` 或 `;`

### 2.8 对象字面量

```zr
// 基本对象
var value = {
    a: 1,
    b: 2,
    c: 3
};

// 使用分号分隔
var valueX = {
    a: 1;
};

// 计算键
var value2 = {
    ["a"]: "a";
    b: "b";
    c: "c";
    [2]: "d";
};
```

**说明**:
- 键可以是标识符、字符串或表达式
- 值可以是任意表达式
- 可以使用 `,` 或 `;` 分隔成员

### 2.9 数组字面量

```zr
// 基本数组
var arr = [1, 2, 3];

// 使用分号分隔
var arr2: int[] = [1; 2; 3;];

// 元组类型
var arr3: [string, int, bool, object] = ["root", 1, true, null];
```

**说明**:
- 可以使用 `,` 或 `;` 分隔元素
- 支持元组类型注解

### 2.10 控制流

#### if 语句

```zr
// if 语句
if (condition) {
    // statements
}

if (condition) {
    // statements
} else {
    // statements
}
```

#### switch 语句

```zr
// switch 语句
switch (value) {
    (1) {
        // case 1
    }
    (2) {
        // case 2
    }
    () {
        // default case
    }
}
```

#### while 循环

```zr
// while 语句
while (condition) {
    // statements
}
```

#### for 循环

```zr
// 传统 for 循环
for (var i: int = 0; i < 100; i += 1) {
    // statements
}

// 无限循环
for (;;) {
    // statements
}

// foreach 循环
for (var i in array) {
    // statements
}
```

#### 生成器表达式

```zr
// 生成器允许在 out 时返回结果，直到运行结束
var k = {{
    if (Object.type(j) == "array") {
        out j.toArray();
    }
    // default out null;
}};
// 函数返回生成器
test(){
    return {{
        if (Object.type(j) == "array") {
            out j.toArray();
        }
        // default out null;
    }}
}
```

**生成器说明**:
- 使用 `{{}}` 语法
- `out` 语句会寻找最近的 block 表达式
- 可以多次 `out`，返回所有结果
- 默认 `out null`

#### try-catch-finally

```zr
// try 语句
try {
    // statements
}

// try-catch
try {
    // statements
} catch (e) {
    // error handling
}

// try-catch-finally
try {
    // statements
} catch (e) {
    // error handling
} finally {
    // cleanup
}
```

### 2.11 表达式

#### 运算符优先级

从高到低：

1. **成员访问**: `.`, `[]`
2. **一元运算符**: `!`, `~`, `+`, `-`, `$`, `new`
3. **乘除模**: `*`, `/`, `%`
4. **加减**: `+`, `-`
5. **位移**: `<<`, `>>`
6. **比较**: `<`, `>`, `<=`, `>=`
7. **相等**: `==`, `!=`
8. **位与**: `&`
9. **位异或**: `^`
10. **位或**: `|`
11. **逻辑与**: `&&`
12. **逻辑或**: `||`
13. **三元运算符**: `? :`
14. **赋值**: `=`, `+=`, `-=`, `*=`, `/=`, `%=`

#### 函数调用

```zr
// 基本调用
functionName(arg1, arg2);

// 方法调用
obj.method(arg1, arg2);

// 动态方法调用（不明确类型，首先 to_function 指令校验转换为函数，然后调用时进行类型检查）
obj["method"](arg1, arg2);

// 链式调用
obj.member["sant"]();

// 复杂链式调用
// revert(mam) 返回为函数，需要 to_function 指令校验转换，才能继续调用("kanji")
// revert(mam)("kanji") 返回 object 才能访问["minify"]
// (obj["Vector3"].revert(mam)("kanji")["minify"] + val) 返回了 struct 原型，才能使用 $xxx(s) 实例化
$(obj["Vector3"].revert(mam)("kanji")["minify"] + val)(s);
```

#### 成员访问

```zr
// . 方式访问优先找成员
obj.property;

// [] 方式访问优先被元方法截获，再找键值对
obj["property"];

// 方法调用
obj.method();

// 动态方法调用（调用前必然经过指令 to_function 校验）
obj["method"]();
```

### 2.12 测试声明

```zr
%test("test_name") {
    // throw result; 结果不符合预期而抛出，测试失败
    // throw false; 直接测试失败
    // test body
    return 0;  // 测试成功，返回预期结果为 0
}
```

**说明**:
- 使用 `%test("name")` 语法
- `return` 值表示预期结果
- `throw` 表示测试失败

### 2.13 中间代码 (intermediate)

```zr
intermediate TestIntermediate(i: int, j: int): int %
<
    // 闭包：会去校验上层同名局部变量
    c1: int,
    c2: int,
    c3: int
>
[
    // 常量：可指定类型
    num = 1;
    str = "test";
    num2 = 2;
]
(
    // 函数栈/局部变量名字，按顺序编码，可不指定类型
    var1: int,
    var2: int
)
{
    GetConstant num;
    SetStack var1;
    GetConstant num2;
    SetStack var2;
    AddInt var1 var2;
    SetStack var1;
    AddInt i j;
    SetStack var2;
    AddInt var1 var2;
    SetStack 1;
    FunctionReturn 0 1;
}
```

**语法结构**:
- `intermediate` 关键字
- 函数签名: `name(params): returnType`
- `%` 分隔符
- 闭包参数: `<...>`（会校验上层同名局部变量）
- 常量: `[...]`（可指定类型）
- 局部变量: `(...)`（按顺序编码，可不指定类型）
- 指令体: `{...}`

### 2.14 装饰器

```zr
// 类装饰器（脚本初始化时立即执行，允许以 zr.ClassDecorator 衍生类或者函数返回作为装饰器结果）
#singleton#
#serializable#
#decorator_with_args("arg")#

class MyClass {
    // 属性装饰器（脚本初始化时立即执行，允许以 zr.PropertyDecorator 衍生类或者函数作为装饰器结果）
    #serializable#
    pub var field: int;
    
    // 范围约束装饰器（编译期和运行时检查）
    #range(min: 0, max: 100)#
    pub var count: int;
    
    // 方法装饰器（脚本初始化时立即执行，允许以 zr.MethodDecorator 衍生类或者函数作为装饰器结果）
    #networked#
    pub method(): int {
        return 1;
    }
}
```

**说明**:
- 装饰器使用 `#...#` 语法
- 装饰器在脚本初始化时立即执行
- 支持类、属性、方法装饰器
- 装饰器可以是标识符或函数调用表达式
- `#range(min: value, max: value)#` 装饰器用于为变量添加范围约束，编译期和运行时都会检查

### 2.15 注释

```zr
// 单行注释

/* 
   多行注释
*/
```

### 2.16 类型注解

```zr
// 基本类型
var name: Type;

// 数组类型
var name: Type[];

// 固定大小数组（编译期检查）
var arr: int[10];  // 固定10个元素的数组

// 范围约束数组（编译期和运行时检查）
var arr2: int[1..100];  // 数组长度必须在1到100之间

// 最小大小约束
var arr3: int[5..];  // 数组长度至少为5

// 元组类型
var name: [Type1, Type2];

// 泛型类型
var name: Generic<Type>;

// 嵌套类型
var name: Outer<Inner<Type>>;
```

---

## 3. 特殊语法特性

### 3.1 元函数/元方法

以 `@` 开头的特殊函数，如 `@constructor`。

```zr
pub @constructor() {
    // 构造函数
}

pub @add(lhs: Type, rhs: Type): Type {
    // 元方法
}
```

### 3.2 值类型构造

使用 `$TypeName(...)` 创建值类型实例。

```zr
// TypeName 可以是 struct 原型 Object
var v = $Vector3(1, 2, 3);

// TypeName 作为表达式，可以是 Object 的原型 Object
var proto = getStructProto();
var v2 = $proto(1, 2, 3);
```

**说明**:
- `$` 操作符用于值类型构造
- TypeName 可以是标识符或表达式
- 表达式必须返回 struct 原型 Object

### 3.3 解构赋值

```zr
// 对象解构
var {a, b, c} = object;

// 数组解构
var [x, y] = array;
var [x] = array;
```

### 3.4 可变参数

```zr
functionName(...args: Type[]) {
    // body
}

functionName(param1: Type, ...args: Type[]) {
    // body
}
```

### 3.5 属性访问器

```zr
// Getter
pub get propertyName {
    return this._value;
}

// Setter
pub set propertyName(val: Type) {
    this._value = val;
}

// 简写形式（接口中）
pub get set propertyName: Type;
```

### 3.6 强制类型转换

```zr
// <> 放在表达式前面表示强制转换，会真实编译为 to_int 指令
var i: int = <int> a;

// 假设 Vector3 为 m 模块声明的结构体，会真实编译为 to_struct 指令
var m = zr.import("math");
var j = <m.Vector3> x;

// 假设 Person 是 k 模块声明的类，会真实编译为 to_object 指令，指令带有 Person 原型信息
var k = zr.import("PersonInfo");
var l = <k.Person> p;
```

**说明**:
- `<Type>` 语法用于强制类型转换
- 会根据目标类型生成不同的转换指令：
  - 基本类型 → `to_int`, `to_float`, `to_string` 等
  - struct → `to_struct`（带原型信息）
  - class → `to_object`（带原型信息）

### 3.7 泛型和动态类型原型

#### 静态泛型

静态泛型为可在编译期明确为基本类型或者已知类/结构体，能进行指令优化。编译期间可以进行类型推断，从而给出更明确的结果。一旦类型是确定的，指令就可以被优化，而且不允许模糊类型进入，或者必须强制转换进入。

```zr
class A<T> {
    pub man: T;
    pub static woman: T;  // 每个泛型在明确后，会产生对应的原型，比如 A<int>，然后 A<int>.woman 里面是 int 类型
}

// 静态泛型编译期即可明确，生成对应的原型
var a: A<int> = new A<int>();
```

#### 动态泛型

动态泛型可以在运行时生效，通过原型里面的动态的 generic 列表明确后取得。

```zr
class A<T> {
    pub man: T;
}

// 动态泛型在第一次遭遇的时候会进行检查并生成，在指令中实现
var a = new A();  // T 在运行时确定
```

**说明**:
- 静态泛型：编译期即可明确，生成对应的原型
- 动态泛型：在第一次遭遇的时候会进行检查并生成，在指令中实现
- 泛型类和函数会维护一个泛型表，通过泛型成员的数量组成键值对

#### 泛型约束

```zr
class C {}

// 泛型约束：T 必须是 C 或其子类
class B<T: C> {}
```

---

## 4. 文件结构

1. **可选的模块声明**
   ```zr
   module "module_name";
   ```

2. **顶层语句（按顺序）**:
   - 结构体声明
   - 类声明
   - 接口声明
   - 枚举声明
   - 变量声明
   - 函数声明
   - 测试声明
   - 表达式语句
   - 中间代码声明

---

## 5. 语法规则总结

### 5.1 基本规则

- **语句分隔**: 语句可以以分号结尾（必须）
- **块表达式**: 块表达式不可返回值，但是生成器可以延迟返回
- **函数重载**: 支持函数重载，函数类型越明确，使用越快的指令集，但是不明确类型调用的时候使用不明确类型函数重载，若不存在则需要强制转换
- **泛型**: 支持泛型（语法支持）

### 5.2 类型系统

- **类型推断**: 支持类型推断，未明确类型的变量推断为 `object`
- **类型注解**: 支持显式类型注解
- **类型转换**: 支持强制类型转换 `<Type>`
- **泛型**: 支持静态泛型和动态泛型
- **类型检查**: 编译期进行类型兼容性检查，运行时（debug模式）可进行额外检查
- **范围约束**: 支持为变量和数组添加范围约束，编译期和运行时都会验证
- **边界检查**: 编译期检查数组索引边界（字面量索引），运行时（debug模式）检查所有索引访问

### 5.3 访问控制

- **访问修饰符**: `pub` (public), `pri` (private), `pro` (protected)
- **默认访问性**: 未指定时默认为 `pri` (private)
- **接口限制**: 接口中只允许 `pub` 和 `pro`，不允许 `pri`

### 5.4 值类型 vs 引用类型

- **struct**: 值类型，使用 `$TypeName(...)` 创建实例
- **class**: 引用类型，使用 `new TypeName(...)` 创建实例
- **struct 不支持继承**: struct 不能继承其他类型
- **class 支持继承**: class 支持单继承和多个接口实现

---

## 6. 类型检查和边界检查

### 6.1 编译期检查

zr 语言在编译期进行以下检查：

1. **类型兼容性检查**: 
   - 赋值操作的类型兼容性
   - 函数调用参数类型匹配
   - 运算符操作数类型检查

2. **字面量范围检查**:
   - 整数字面量是否在目标类型范围内（如 `int8 = 128` 会报错）
   - 无符号整数不能为负数
   - 浮点数字面量的有效性检查

3. **数组大小约束检查**:
   - 数组字面量大小是否符合声明约束
   - 固定大小数组的字面量必须匹配声明的大小

4. **数组索引边界检查**:
   - 字面量索引的编译期越界检查
   - 负数索引检查

### 6.2 运行时检查（Debug模式）

在运行时，如果启用了 debug 模式检查，会进行以下额外验证：

1. **边界检查**: 数组访问时的索引边界验证
2. **类型检查**: 类型转换和赋值时的类型验证
3. **范围检查**: 变量赋值时的范围约束验证

可以通过 `SZrState` 的以下标志控制运行时检查：
- `enableRuntimeBoundsCheck`: 启用运行时边界检查
- `enableRuntimeTypeCheck`: 启用运行时类型检查
- `enableRuntimeRangeCheck`: 启用运行时范围检查

### 6.3 范围约束语法

```zr
// 整数范围约束（使用装饰器）
#range(min: 0, max: 100)#
var count: int;

// 数组大小约束（使用类型注解）
var arr: int[10];        // 固定大小
var arr2: int[1..100];   // 范围约束
var arr3: int[5..];      // 最小大小

// 编译期检查字面量范围
var x: int8 = 128;       // 编译错误：超出范围
var y: uint8 = -1;       // 编译错误：负数不能赋值给无符号类型
```

## 7. 已知问题和待实现功能

### 7.1 Lexer 问题

1. **字符字面量识别问题**: 
   - 当前 lexer 将单引号和双引号都当作字符串处理
   - `read_char` 函数已实现但未被调用
   - 需要修复：单引号应识别为字符字面量 `ZR_TK_CHAR`，双引号识别为字符串字面量 `ZR_TK_STRING`

### 7.2 Parser 待实现功能

1. **类型转换语法**: 
   - `<Type>` 类型转换语法在 parser 中可能没有明确支持
   - 需要添加对类型转换表达式的解析支持

2. **字符字面量解析**: 
   - 如果 lexer 修复了字符字面量识别，parser 需要确保能正确解析 `ZR_TK_CHAR` token

### 7.3 测试覆盖

建议为以下功能添加单元测试：

1. 字符字面量的各种转义序列
2. 类型转换 `<Type>` 的各种场景
3. 泛型约束语法
4. 装饰器的各种用法
5. 中间代码的各种指令
6. 类型检查和边界检查的各种场景

---

## 附录

### A. 运算符优先级表

| 优先级 | 运算符 | 结合性 |
|--------|--------|--------|
| 1 | `.` `[]` | 左结合 |
| 2 | `!` `~` `+` `-` `$` `new` | 右结合 |
| 3 | `*` `/` `%` | 左结合 |
| 4 | `+` `-` | 左结合 |
| 5 | `<<` `>>` | 左结合 |
| 6 | `<` `>` `<=` `>=` | 左结合 |
| 7 | `==` `!=` | 左结合 |
| 8 | `&` | 左结合 |
| 9 | `^` | 左结合 |
| 10 | `\|` | 左结合 |
| 11 | `&&` | 左结合 |
| 12 | `\|\|` | 左结合 |
| 13 | `? :` | 右结合 |
| 14 | `=` `+=` `-=` `*=` `/=` `%=` | 右结合 |

### B. 关键字列表

**类型声明**: `module`, `struct`, `class`, `interface`, `enum`, `intermediate`

**变量和函数**: `var`, `static`

**访问修饰符**: `pub`, `pri`, `pro`

**控制流**: `if`, `else`, `switch`, `while`, `for`, `break`, `continue`, `return`

**异常处理**: `try`, `catch`, `finally`, `throw`

**特殊关键字**: `new`, `super`, `in`, `out`, `get`, `set`

**测试**: `test`

**特殊值**: `Infinity`, `NegativeInfinity`, `NaN`, `true`, `false`, `null`

### C. 示例文件索引

- `tests/parser/test_simple.zr` - 完整语法示例
- `tests/scripts/test_cases/classes.zr` - 类相关示例
- `tests/scripts/test_cases/enums.zr` - 枚举示例
- `tests/scripts/test_cases/decorators.zr` - 装饰器示例
- `tests/scripts/test_cases/lambda.zr` - Lambda 表达式示例
- `tests/scripts/test_cases/intermediate.zr` - 中间代码示例

---

**文档版本**: 1.0  
**最后更新**: 2025-01-XX  
**基于**: zr_vm parser implementation and test_simple.zr


