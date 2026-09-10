---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_meta_conf.h
  - zr_vm_common/include/zr_vm_common/zr_type_conf.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_interface.c
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/src/zr_vm_core/value.c
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
  - zr_vm_core/src/zr_vm_core/execution/execution_numeric.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class.c
  - zr_vm_core/src/zr_vm_core/value.c
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
  - zr_vm_core/src/zr_vm_core/execution/execution_numeric.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/zr_language_specification.md
tests:
  - tests/meta/test_meta.c
  - tests/parser/test_parser.c
  - tests/parser/test_syntax_reference_v1.c
  - tests/fixtures/projects/syntax_reference_v1/src/object_model.zr
  - tests/fixtures/projects/syntax_reference_v1/src/ownership.zr
doc_type: language-reference
---

# 运算符、元方法与类型转换

本页把源码运算符与运行时元方法放在同一张契约中说明。解析器只负责建立表达式形状；
真正的操作数类型、数值提升、可写 Place、元方法可见性和失败策略由 semantic phase
与 core execution 决定。这样同一套规则可以被解释器、AOT C、AOT LLVM、反射和 native
provider 复用。

## 1. 源码优先级

当前 production parser 从高到低使用以下层级。表中的结合性是 AST 组合方式，不是运行时
副作用承诺。

| 优先级 | 语法 | 结合性 | 结果或限制 |
| --- | --- | --- | --- |
| 1 | ., ?., [], (), <T>(...), ?.(...) | 左 | postfix 链；optional 链缺失 receiver 时短路剩余链。 |
| 2 | typeid(...), typeof(...), await, ref, init, new, own | 右 | 专用前缀；操作数类别由 parser 固定。 |
| 3 | !, ~, 一元 +, 一元 - | 右 | unary expression；不能把二元 - 当成同一 AST 节点。 |
| 4 | *, /, % | 左 | 乘除余；除零和范围由 execution lane 检查。 |
| 5 | +, - | 左 | 加减；字符串等类型可由元方法扩展。 |
| 6 | <<, >> | 左 | 移位；移位计数的范围由数值 lane 检查。 |
| 7 | <, >, <=, >= | 左 | relational；优先使用 typed numeric lane，必要时调用 COMPARE。 |
| 8 | ==, != | 左 | 严格值相等/不等；core 先比较 value type。 |
| 9 | & | 左 | bitwise and。 |
| 10 | ^ | 左 | bitwise xor；不要把它当成幂运算语法。 |
| 11 | \| | 左 | bitwise or。 |
| 12 | && | 左 | logical and；右侧按需求值。 |
| 13 | || | 左 | logical or；右侧按需求值。 |
| 14 | ?: | 右 | conditional；只求值选中的分支。 |
| 15 | =, +=, -=, *=, /=, %= | 右 | assignment；左侧必须能绑定到可写 Place。 |

ZR 没有自动分号插入。每条语句仍需显式分号，换行不会改变上表优先级。括号会保留
source range，因此既可改变组合，也可改善 diagnostic 和 LSP 的定位。

~~~zr
let a = first + second * third;
let b = ready && count > 0 || fallback;
let c = ok ? "yes" : "no";
total += step * 2;
~~~

这些表达式分别组合成 first + (second * third)、(ready && (count > 0)) || fallback、
ok ? "yes" : "no" 和 total = total + (step * 2) 的语义形状；复合赋值是否真的重读
左侧值，还要看该 Place 的 property/index contract。

## 2. 元方法槽位

公共头文件 zr_meta_conf.h 通过一个宏同时生成 EZrMetaType 枚举和 CZrMetaName 字符表。
当前槽位如下。self 是隐含的第一个参数，普通二元槽位最多再接一个显式参数。

| 槽位 | 常见触发点 | 形状 | 说明 |
| --- | --- | --- | --- |
| CONSTRUCTOR | init T(...)、对象构造 | self + args | 初始化目标；失败必须走未完成对象清理。 |
| DESTRUCTOR | resource/drop | self | 释放资源；应幂等，不能把异常吞成成功。 |
| ADD | a + b | self, rhs | 加法或类型自定义组合。 |
| SUB | a - b | self, rhs | 减法。 |
| MUL | a * b | self, rhs | 乘法。 |
| DIV | a / b | self, rhs | 除法；零除仍由实现检查。 |
| MOD | a % b | self, rhs | 余数。 |
| POW | provider/数值扩展 | self, rhs | 幂运算槽位；当前 ^ 是 bitwise xor。 |
| NEG | -a | self | 一元负号。 |
| COMPARE | relational/排序 | self, rhs | 约定负/零/正的三路比较。 |
| TO_BOOL | 条件、! | self | 转为 bool。 |
| TO_STRING | console、插值、异常文本 | self | 返回 string；错误结果触发 core fallback。 |
| TO_INT | 显式/隐式整数转换 | self | 返回有符号整数。 |
| TO_UINT | 显式无符号转换 | self | 返回无符号整数。 |
| TO_FLOAT | 显式浮点转换 | self | 返回 float/double lane 支持的值。 |
| CALL | callable value 后缀调用 | self, args | 使对象可调用。 |
| GETTER | property 读取 | self 或 self, key | 由 property descriptor 绑定。 |
| SETTER | property 写入 | self, value | 验证可写性和 value type。 |
| SHIFT_LEFT | a << b | self, rhs | 左移。 |
| SHIFT_RIGHT | a >> b | self, rhs | 右移。 |
| BIT_AND | a & b | self, rhs | 位与。 |
| BIT_OR | a \| b | self, rhs | 位或。 |
| BIT_XOR | a ^ b | self, rhs | 位异或。 |
| BIT_NOT | ~a | self | 位反。 |
| GET_ITEM | a[key] | self, key | 索引读取。 |
| SET_ITEM | a[key] = value | self, key, value | 索引写入。 |
| CLOSE | using/close | self | 资源或 guard 的关闭协议。 |

类或接口中用 @ 声明元方法。例如：

~~~zr
class Vector {
    pub @add(other: Vector): Vector { return Vector(x + other.x, y + other.y); }
    pub @toString(): string { return "Vector(" + x + "," + y + ")"; }
}

interface Indexable<T> {
    @getItem(index: int): T;
    @setItem(index: int, value: T): void;
}
~~~

@decorate 已被 parser 明确移除；声明变换应使用 comptime declaration-transform provider。
元方法名字本身会进入 semantic metadata，不能只靠普通同名 method 绕过 receiver、arity
或访问检查。

## 3. 调用帧和 dispatch

ZrCore_Value_GetMeta(state, value, metaType) 按以下顺序查找：

1. 对象沿 prototype/superPrototype 链查找；没有自己的槽位时再看基本 object prototype。
2. 基本值从 global->basicTypeObjectPrototype[type] 取得对应槽位。
3. NATIVE_DATA 当前没有默认 meta table，找不到时返回 NULL。

找到槽位后，ZrCore_Value_CallMetaMethod 会复制稳定的 self，保存当前 stack/call-info，
在 scratch slots 中布置 function + self + arguments，再调用普通 function execution。公共
宏给出的布局不变量是 self slot = base + 1、第二个显式位置 = base + 2、最大显式参数数
为 2。调用结束后必须恢复 stack top 和 call-info；否则后续异常或 GC 会看到错误 frame。

解释器在 typed path 能证明类型时直接执行数值指令；无法证明时才进入 meta/dynamic path。
AOT backend 可以生成快路径，但必须保留相同的 meta fallback 和 contract guard。callback
返回 false、写入错误类型或违反 arity 时，caller 不能把未初始化的 result 当作成功值。

## 4. 相等、比较和哈希

ZrCore_Value_Equal 要求两个值的 EZrValueType 相同，然后按类别比较：

| 类别 | 相等依据 |
| --- | --- |
| null | 任意两个 null 相等。 |
| bool | 布尔位相同。 |
| signed/unsigned integer | 同类整数的 native 数值相同；不同 value type 先失败。 |
| float/double | native 浮点相等；NaN 遵循 C 比较。 |
| string | 字符串内容相等。 |
| native pointer/data | 指针地址相等。 |
| GC object | object identity 相等。 |

ZrCore_Value_CompareDirectly 明确不调用 meta function，用于 hash set 的 key 查找和内部
身份判断；它不等价于用户可见的 compareTo。排序 provider 可以在无法直接比较时调用
COMPARE 或自己的 comparer。ZrCore_Value_GetHash 与 equality 的 key 规则必须一致；自定义
类型改变 equality 时，也必须同步提供稳定 hash。

## 5. 转换规则

核心执行器对基础类型先走无分配的转换：

| 目标 | 已知输入 | 处理 |
| --- | --- | --- |
| TO_INT | signed integer | 原值复制。 |
| TO_INT | unsigned integer | 按 TZrInt64 转换。 |
| TO_INT | float | 截断为 TZrInt64。 |
| TO_INT | bool | false -> 0，true -> 1。 |
| TO_UINT | unsigned integer | 原值复制。 |
| TO_UINT | signed integer/float/bool | 按 TZrUInt64 转换。 |
| TO_FLOAT | float/double | 原值复制。 |
| TO_FLOAT | signed/unsigned integer | 转换为 TZrFloat64。 |
| 任一目标 | 其它对象 | 先查对应 TO_* meta；没有或返回类型错误时使用 lane 零值回退。 |

因此“语法上可写 cast”不等于“任意对象都能安全转换”。范围、精度、负数转无符号和
provider 自定义错误应由类型系统或 callback 明确处理；表格中的 T? 不是 ZR 源码类型
后缀。布尔条件会查 TO_BOOL，基础测试验证了 null、零、空字符串为 false，非零、非空
值为 true；自定义类型应返回真正的 bool。

~~~zr
let i: int = <int>3.9;
let u: uint = <uint>i;
let f: float = <float>u;
let text: string = value.toString();
~~~

显式 cast 的 TypeRef 必须能被 parser 完整解析，且 > 后必须能开始一个表达式。$Type(...)
历史构造形式不属于当前 production grammar。semantic phase 还会检查 target type 的
ownership/layout；不能用 cast 绕过 ref、scoped、resource 或跨 domain transfer 规则。

## 6. Getter、setter、index 和 callable

property 的读写不是普通字段别名。GETTER/SETTER descriptor 记录 reference access、是否
导出 writable ref 以及 getter/setter 的 callable TypeId。ZrCore_Object_GetMember、SetMember、
GetByIndex 和 SetByIndex 在进入 meta 之前会检查 receiver、owner prototype、访问权限和
write barrier。getter 返回 borrowed view 时，调用者不能把它保存过当前 region；setter 失败
时，原值必须保持不变。

可调用对象先查 CALL meta，再进入 closure/native callable 路径。callback?.(arg) 的 optional
call 会在构造参数之前检查 callback 是否为 null；这保证被跳过的参数表达式不产生副作用。

## 7. C 调用示例

宿主想主动执行元方法时，使用 core API；不要手工改 stack slot：

~~~c
SZrTypeValue value;
SZrTypeValue argument;
SZrTypeValue result;
ZrCore_Value_InitAsInt(state, &value, 10);
ZrCore_Value_InitAsInt(state, &argument, 20);

if (!ZrCore_Value_CallMetaMethod(state, &value, ZR_META_ADD,
                                 &result, 1u, &argument)) {
    /* no ADD implementation or callback failure */
}

SZrMeta *toString = ZrCore_Value_GetMeta(state, &value, ZR_META_TO_STRING);
if (toString != ZR_NULL) {
    SZrString *text = ZrCore_Value_ConvertToString(state, &value);
    /* text is state-owned and borrowed */
    (void)text;
}
~~~

ZrCore_Value_CallMetaMethod 的可变参数只承载最多两个显式槽位；更多用户参数应走 CALL
或普通 callable API。result 必须是可写且已初始化的 SZrTypeValue；如果它原来持有
unique/shared/loaned ownership，core overwrite path 会先 release。跨 safepoint 保存结果时，
使用 root/pin 或 ZrCore_Value_Copy，不能保存 callback 内部的临时指针。

## 8. 常见失败矩阵

| 现象 | 根因 | 正确处理 |
| --- | --- | --- |
| a + b 找不到实现 | 两个操作数都没有可用 ADD，或类型不兼容 | 提供 typed overload/meta，或在 semantic 层报告类型错误。 |
| 自定义 toString 后仍显示默认文本 | callback 返回非 string 或调用失败 | 检查 result type 和异常；core 会回退，不会猜测转换。 |
| Map 中相等对象查找失败 | equality/hash 不一致，或使用了 wrong generation | 提供稳定 hash，并检查 metadata generation。 |
| x[i] = v 被拒绝 | Place 不可写、setter 缺失、借用冲突 | 使用 writable receiver 或显式 copy；不要绕过 SetByIndex。 |
| await 后引用失效 | loan/ref 跨挂起或 frame 无法保留 | 把值复制/转移为 frame-safe owner。 |
| callback 偶发崩溃 | 保存了 borrowed meta/argument pointer 跨 GC | 使用 temp root、native pin，并在 safepoint 后重新获取。 |

### 证据边界

本页描述的是当前 checkout 中公共头文件和 execution source 已实现的 contract。某个
provider 是否为具体类型安装了 ADD、GET_ITEM 或 CLOSE，仍要以该 provider descriptor 和
项目 phase 为准；不要仅凭 EZrMetaType 枚举值推断所有类型都支持全部运算符。
