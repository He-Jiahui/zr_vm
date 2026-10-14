---
related_code:
  - zr_vm_lib_math/include/zr_vm_lib_math/module.h
  - zr_vm_lib_math/include/zr_vm_lib_math/scalar.h
  - zr_vm_lib_math/include/zr_vm_lib_math/vector2.h
  - zr_vm_lib_math/include/zr_vm_lib_math/vector3.h
  - zr_vm_lib_math/include/zr_vm_lib_math/vector4.h
  - zr_vm_lib_math/include/zr_vm_lib_math/complex.h
  - zr_vm_lib_math/include/zr_vm_lib_math/complex_registry.h
  - zr_vm_lib_math/include/zr_vm_lib_math/quaternion.h
  - zr_vm_lib_math/include/zr_vm_lib_math/matrix3x3.h
  - zr_vm_lib_math/include/zr_vm_lib_math/matrix4x4.h
  - zr_vm_lib_math/include/zr_vm_lib_math/tensor.h
  - zr_vm_lib_math/include/zr_vm_lib_math/math_common.h
  - zr_vm_lib_math/src/zr_vm_lib_math/module.c
  - zr_vm_lib_math/src/zr_vm_lib_math/common.c
  - zr_vm_lib_math/src/zr_vm_lib_math/scalar/scalar.c
  - zr_vm_lib_math/src/zr_vm_lib_math/scalar/scalar_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector3.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector3_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector4.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector4_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c
  - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/quaternion/quaternion.c
  - zr_vm_lib_math/src/zr_vm_lib_math/quaternion/quaternion_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/matrix/matrix3x3.c
  - zr_vm_lib_math/src/zr_vm_lib_math/matrix/matrix3x3_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/matrix/matrix4x4.c
  - zr_vm_lib_math/src/zr_vm_lib_math/matrix/matrix4x4_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/tensor/tensor.c
  - zr_vm_lib_math/src/zr_vm_lib_math/tensor/tensor_registry.c
implementation_files:
  - zr_vm_lib_math/src/zr_vm_lib_math/module.c
  - zr_vm_lib_math/src/zr_vm_lib_math/common.c
  - zr_vm_lib_math/src/zr_vm_lib_math/scalar/scalar.c
  - zr_vm_lib_math/src/zr_vm_lib_math/scalar/scalar_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector3.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector3_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector4.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector4_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c
  - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/quaternion/quaternion.c
  - zr_vm_lib_math/src/zr_vm_lib_math/quaternion/quaternion_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/matrix/matrix3x3.c
  - zr_vm_lib_math/src/zr_vm_lib_math/matrix/matrix3x3_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/matrix/matrix4x4.c
  - zr_vm_lib_math/src/zr_vm_lib_math/matrix/matrix4x4_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/tensor/tensor.c
  - zr_vm_lib_math/src/zr_vm_lib_math/tensor/tensor_registry.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - user: 2026-09-26 全仓库首方代码调用链审查与注释任务
  - docs/library-and-builtins/index.md
tests:
  - tests/fixtures/projects/native_numeric_pipeline/src/tensor_pipeline.zr
  - tests/fixtures/projects/native_numeric_pipeline/src/signal.zr
  - tests/fixtures/projects/native_math_export_probe/src/main.zr
  - tests/library/test_official_provider_convergence.c
doc_type: api-reference
---

# `zr.math` API 参考

`zr.math` 是 Runtime provider，常量和函数以 `float`/`double` 兼容的数值 value 工作，向量、
矩阵、复数和四元数使用值语义。运算符 meta method 会创建新值，不修改左操作数；Tensor
则是 managed object，内部数据按 row-major 连续数组保存。

## 常量和标量函数

| 名称 | 语义 |
| --- | --- |
| `PI`、`TAU`、`E` | 圆周率、2*pi、自然常数。 |
| `EPSILON` | provider 的近似比较阈值。 |
| `INF`、`NAN` | IEEE infinity/NaN。 |

| 函数 | 签名/公式 |
| --- | --- |
| `abs` | `abs(x)`，绝对值。 |
| `min` / `max` | `min(a,b)` / `max(a,b)`。 |
| `clamp` | `clamp(value, lower, upper)`，按传入顺序先应用下界、再应用上界；调用方应保证 `lower <= upper`。 |
| `lerp` | `lerp(a,b,t) = a + (b-a)*t`。 |
| `sqrt` / `rsqrt` | `sqrt(x)`；`rsqrt(x)=1/sqrt(x)`。 |
| `pow` / `exp` / `log` | 委托宿主 C math；回调直接返回其浮点计算结果。 |
| `sin`、`cos`、`tan` | 弧度制三角函数。 |
| `asin`、`acos`、`atan`、`atan2` | 反三角函数；`atan2(y,x)` 保留象限。 |
| `floor`、`ceil`、`round`、`sign` | 舍入和符号。 |
| `degrees` / `radians` | 角度和弧度转换。 |
| `almostEqual` | `almostEqual(lhs,rhs,epsilon?:float): bool`，比较绝对差 `abs(lhs-rhs) <= epsilon`，缺省使用 EPSILON。 |
| `invokeCallback` | `invokeCallback(callback,value)`，用于验证 callable binding。 |

```zr
let math = import("zr.math");
let t = math.clamp(alpha, 0.0, 1.0);
let angle = math.radians(90.0);
if (math.almostEqual(math.sin(angle), 1.0)) {
    math.invokeCallback((x: float) => x * x, 3.0);
}
```

scalar 实现使用宿主 `<math.h>`；NaN 不应直接参与 `almostEqual` 的 true 判定，调用者需要
先检查 `x == x` 或显式处理非有限值。归一化函数在长度不超过 `ZR_MATH_EPSILON` 时返回
零向量，避免除零。

## Vector2/3/4

三个类型字段分别为 `Vector2.x/y`、`Vector3.x/y/z`、`Vector4.x/y/z/w`。公共方法：

| 方法 | 说明 |
| --- | --- |
| `length` / `lengthSquared` | 欧氏长度及平方长度。 |
| `normalized` | 长度足够大时除以长度；否则返回零值。 |
| `dot(other)` | 点积。 |
| `distance(other)` | 两点距离。 |
| `lerp(other,t)` | 分量线性插值。 |
| `add/sub/neg` meta | `+`、`-`、一元负号。 |
| `compare` meta | 比较平方长度，避免不必要开方。 |
| `toString` meta | 格式化为调试文本。 |

`Vector3` 额外提供 `cross(other)`。构造器按字段顺序接收完整分量：Vector2/3/4
分别要求 2/3/4 个 `float` 参数。

## Complex 和 Quaternion

`Complex(real, imag)` 要求两个数值实参；实例具有可读数值 `real`、`imag` 字段。
`magnitude()` 返回模长，`phase()` 返回弧度相角；`conjugate()`、`normalized()` 和算术 meta
返回新对象并保留 receiver，构造器则可能复用 binding 的兼容目标。模长不超过 EPSILON
时 normalized 返回零复数。乘法公式为 `(a+bi)(c+di)=(ac-bd)+(ad+bc)i`。

compare 按模长平方返回 -1/0/1，不比较分量字典序；**TODO:** NaN 或双方平方溢出时会
落到 0，需要结合 VM compare 消费规则和边界用例确定 unordered 的策略。
**BUG:** 有限 `(1e200, 0)` 的平方中间值溢出，使 magnitude 得到无穷、normalized 得到
零复数。这是当前源码可证实的数值限制，本次未新增运行复现。

C 回调仅借用本次 binding 的 context/state/result；返回对象和字符串归 VM 管理。
缺参/实参标签错误会进入 VM 异常，字段读取或对象 helper 返回 NULL 是正常失败。
**BUG:** toString 生成未驻留短串、对象创建成功而 HASH_BUCKET 扩容失败时，字符串
setter 不写结果槽，helper 仍返回 true；常规 VM 派发预置 NULL，因此可能成功但无字符串结果。

`Quaternion` 字段 `x/y/z/w`；方法 `length`、`lengthSquared`、`normalized`、`conjugate`、
`inverse`、`dot`、`mul`、`slerp`。接近零长度时 inverse/normalized 返回单位四元数；slerp
对负 dot 翻转右操作数，并在 dot 大于 `0.9995` 时改用线性插值；调用方应传入单位
四元数，因为当前实现不归一化输入，也不对 dot 进行夹紧。

```zr
let q0 = init math.Quaternion(0.0, 0.0, 0.0, 1.0);
let q1 = init math.Quaternion(0.0, 0.0, 1.0, 0.0);
let halfway = q0.slerp(q1, 0.5);
```

当前 descriptor 注册了 constructor、算术 meta 和上表列出的实例方法。

Quaternion 回调按字段名读取数值快照，不核对 prototype，也不统一要求单位范数；
`slerp` 的单位端点要求仍适用。context、self/参数视图和 result 仅在本次 native 回调内
借用；构造可复用兼容实例，运算结果对象交给 VM 管理。缺参或实参标签错误进入 VM
异常，字段读取或对象 factory 返回 NULL 则可使回调返回失败。

**BUG:** 合法有限输入 `(1e200,0,0,0)` 的中间平方范数溢出：`length` 返回无穷，
`normalized` 返回零四元数，`inverse` 把可表示的 `-1e-200` 分量算成零。上述结果可由
构造、字段读取和当前公式的完整路径证实，本批没有运行数值边界复现或修改算法。

**BUG:** 对未驻留的短字符串，短串表扩桶的 `HASH_BUCKET` 原生分配返回 NULL 且没有
VM 异常时，字符串 setter 保留 dispatch 预置的 null，格式化 helper 和 `toString` 回调
仍报告成功。完整 descriptor → dispatch → 字符串表 → 分配器证据见
[Quaternion 审查台账](../../code-review/coverage/zr_vm_lib_math_quaternion.tsv)。该结论是
静态调用路径证明，本批未做分配失败注入。

**BUG:** 新建实例的字段哈希表初始容量 4、负载阈值 3，按 `x/y/z/w` 填充时第四次写入
需要扩桶。如果 pin、键字符串创建均成功，而宿主分配器对该 `HASH_BUCKET` 扩容返回
NULL 并保留旧块、VM 没有异常，void setter 只记录日志；factory 不检查状态，构造或
运算回调仍可交付缺少 `w` 的对象。构造 helper 还可复用兼容 self，上述条件限定到
空字段表的填充路径。完整静态路径见同一审查台账，未做故障注入。

**TODO:** 应以单位近邻端点核实 `slerp` 的单位输出需求和 `t` 区间；当前文档未承诺这两项。

## Matrix3x3/4x4

矩阵字段按 row-major 暴露：`m00 ... m22` 或 `m00 ... m33`。两者都提供
`identity`、`transpose`、`determinant`、`inverse`、`mulVector`、`mulMatrix` 和 meta `mul`/
`toString`。Matrix4x4 还提供静态 `translation(x,y,z)`、`scale(x,y,z)`、`rotationX/Y/Z(angle)`。

```zr
let model = math.Matrix4x4.translation(1.0, 2.0, 0.0)
           .mulMatrix(math.Matrix4x4.rotationZ(math.radians(45.0)));
let world = model.mulVector(position);
```

当前实现以固定阈值判定逆矩阵计算失败，并从 native 回调返回失败；矩阵乘法顺序不可
交换，文档示例按实现的 row-major 约定解释。

## Tensor

`Tensor` 字段：`shape: array`、`rank: int`、`size: int`。构造器 descriptor 形状为
`Tensor(shape: array, fillValue: float)`，但当前 native 回调实际读取 `shape` 与 `data`
两个数组，要求 data 长度等于维度乘积，并建立 row-major storage；descriptor 的第二
参数签名文字与实现不一致。调用时传入完整数据数组：

```zr
let tensor = new math.Tensor([2, 3], [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]);
tensor.set([0, 1], 4.0);
let value = tensor.get([0, 1]);
let transposed = tensor.transpose2D();
let product = tensor.matmul(transposed);
```

| 方法 | 规则 |
| --- | --- |
| `clone` | 复制 shape 和 data 数组容器，返回新 Tensor。 |
| `reshape(shape)` | 新 shape 的元素总数必须与 size 相同；返回 shape 和 data 均复制的新 Tensor。 |
| `fill(value)` | 原位覆盖全部元素，并返回 receiver。 |
| `get(indices)` / `set(indices,value)` | indices 数组长度必须等于 rank；按 row-major 计算 offset；set 原位更新并返回 receiver。 |
| `sum` / `mean` | 全量聚合；mean 要求 size 大于零，否则 native 回调失败。 |
| `transpose2D` | 仅 rank=2；返回数据重排后的新 Tensor。 |
| `matmul` | 两个 rank=2，左列数等于右行数。 |
| `add` / `sub` | shape 完全相同。 |
| `mulScalar` | 每元素乘一个 scalar。 |
| `toArray` | 返回数据副本，不暴露内部 storage。 |

实现会检查 shape 各维为正、data 长度、indices 数量和索引范围，以及 matmul 的二维尺寸。
当前整数读取会把浮点维度与索引截断，构造器也未逐项验证 data 是否为数值；因此调用方
应传整数维度、整数索引和数值数据。负索引会被拒绝。

## 计算和对象分配

向量、矩阵、Complex 和 Quaternion 的运算回调创建新结果对象。Tensor 的 `clone`、
`reshape`、算术运算和转置返回新对象；`fill` 与 `set` 修改并返回原对象。

## C 注册入口

```c
const ZrLibModuleDescriptor *math = ZrVmLibMath_GetModuleDescriptor();
if (!ZrVmLibMath_Register(global)) {
    return ZR_FALSE;
}
```

native callback 读写数学值时使用 `ZrLib_CallContext_ReadFloat` 和
`ZrLib_Value_SetFloat`；不要假定 `SZrTypeValue` 的 union 与 `double` 对齐。若需要把自定义
struct 暴露为 Vector-like value，必须提供完整 field/layout/protocol descriptor，并让
`ZrLibrary_NativeRegistry_ComputeModuleSignatureHash` 反映变化。

## Complex C binding 调用约束

Complex 的公开 C 声明包含构造、4 个命名方法和 6 个运算元方法。脚本先导入已注册的
zr.math，再通过方法或运算协议进入同步 binding；不把这些 C 回调当作脱离 VM 域的
独立数学函数。zr_vm_lib_math/src/zr_vm_lib_math/module.c:129 聚合类型；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:202、zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:205 选择方法回调，
zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:274 完成 native GC 准入后才在 zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277 执行。

### 参数、结果与失败

| 入口 | 调用方提供 | 成功结果和设计目的 |
| --- | --- | --- |
| Construct | 两个数值 real、imag；binding 的构造目标 | 沿 owner/construct target 初始化对象，兼容 self 可复用。 |
| Magnitude、Phase | 无显式实参；可读数值分量的 self | 模长或弧度相角，供数值流程使用。 |
| Conjugate、Normalized | 无显式实参；可读数值分量的 self | 新 Complex，保留原 receiver；近零模交付零复数。 |
| MetaAdd、MetaSub、MetaMul | self 与一个对象或数组 rhs，两者分量可读为数值 | 新 Complex；rhs 只检查可读字段，不额外核名义原型。 |
| MetaNeg | 可读数值分量的 self，无显式实参 | 符号相反的新 Complex。 |
| MetaCompare | self 与一个分量可读的对象或数组 rhs | 模长平方顺序的 int -1/0/1；0 不能证明分量相等。 |
| MetaToString | 可读数值分量的 self，无显式实参 | VM 管理的调试字符串，不借用格式化栈缓冲。 |

context、state、参数视图和 result 仅借用本次调用。结果对象和字符串由 VM 管理；
跨调用保存裸指针仍须由宿主维持根。构造目标可复用，其余对象运算创建独立结果。
正常 false 不应消费 result；缺参或实参标签不符经 zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:862、zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:877 的
不返回错误入口报告，分配产生的非局部异常也不等于正常 false。字段 setter 为 void，
构造及新对象 helper 没有逐字段成功凭据，应沿已有 setter 失败记录审查故障路径。

### 静态注册数据

GetType 返回模块持有的静态描述符与嵌套表；GetFunctions 返回 NULL 并在非空 count
写 0，实例方法仍由类型表发布；GetHints 返回一项静态字段形状提示，count 可为 NULL。
调用方不释放、不修改这些数据，模块卸载后不能继续保存借用指针。三入口只交付元数据，
分别由 zr_vm_lib_math/src/zr_vm_lib_math/module.c:129、zr_vm_lib_math/src/zr_vm_lib_math/module.c:117、zr_vm_lib_math/src/zr_vm_lib_math/module.c:142 聚合，不执行数值回调。

### 已确认问题与待核查边界

- BUG: 合法有限 (1e200, 0) 的平方在 zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:20 和 zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:39 溢出，使模长为无穷、
  归一化结果为零；结论来自完整数值调用路径，不代表本批新增执行复现。
- TODO: zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:89 对 NaN 和双边平方溢出也可给 0；需追踪 VM compare 消费策略和
  数值边界用例，确认 unordered/溢出与等模的区分意图。
- BUG: 有效 state/receiver 生成未驻留短串，且短串对象创建成功而 HASH_BUCKET 扩容失败时，
  zr_vm_core/src/zr_vm_core/hash_set.c:181 → zr_vm_core/include/zr_vm_core/hash_set.h:537 → zr_vm_core/src/zr_vm_core/string.c:808 正常返回 NULL；
  zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1313 不写结果槽，但 zr_vm_lib_math/src/zr_vm_lib_math/common.c:519 仍 true；常规派发在 zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362 预置 NULL。此为合法失败路径的静态证明，未执行故障注入。

tests/fixtures/projects/native_numeric_pipeline/src/signal.zr:5、
tests/fixtures/projects/native_numeric_pipeline/src/signal.zr:6、
tests/fixtures/projects/native_numeric_pipeline/src/signal.zr:7 与
tests/fixtures/projects/native_numeric_pipeline/src/signal.zr:10 提供正常脚本调用入口。
本批静态复核与编译消费者检查不证明这些脚本已经执行，不赋予故障注入或整库测试通过。


## Vector2 调用边界与已核实限制

Vector2 的 constructor 取两个数值实参；具名 method 与 meta 的固定参数数目由 native dispatcher 在 callback 前检查，数量不符抛 `ArityError`。进入 callback 后，数值或对象标签不符通过 `ReadFloat` / `ReadObject` 抛 `TypeError`；对象/数组中的 `x`、`y` 字段形状不符则返回 false。false 只有在线程状态仍为 FINE 时转成 null，异常状态走清理与传播。`dot`、`distance`、`lerp` 和二元 meta 读取 rhs 的数值字段，不要求同一名义 prototype；`lerp` 不夹紧 factor，允许外推。

构造 helper 复用与 owner prototype 兼容的 receiver，否则按 construct target/owner prototype 创建实例。`normalized`、`lerp`、加减与取负则通过 factory 新建 Vector2，不修改输入。context、receiver、参数和 result 槽仅借用本次同步调用；结果对象和字符串归 VM 管理，静态 descriptor、hint、嵌套表、字面量与函数地址必须随 provider image 保持有效。

**BUG:** `length` 与 `normalized` 对有限 `(1e200, 0)` 先计算平方和，中间值溢出使长度为无穷、归一化结果为零；`distance` 对该向量和零向量同样丢失有限距离。`lerp((1e308,0),(-1e308,0),0.5)` 的差先溢出，返回负无穷分量而数学中点为零。`lengthSquared` 直接返回平方和，其不可表示平方值的溢出与上述有限结果丢失分别记录。当前结论来自实际表达式与调用路径，未运行数值复现。

**BUG:** factory 创建空实例后调用 void 字段 setter；满 ignored-root 表的 RawMalloc 扩表失败不置 VM 异常，却可使 setter 跳过字段，factory 仍发布非空对象。编译器生成的 `CREATE_OBJECT` / `TO_STRUCT` 加 `__constructor` FUNCTION_CALL 另有合法数值 fast-lane 路径：receiver 未被 lane pin，ignored-root 仅余一槽时 setter 先 pin receiver，再为未登记字段 key 扩表；失败后构造 helper 仍可成功返回缺字段对象。这两条路径见 `common.c:22-35,200-252`、`native_binding_dispatch.c:1385,1400` 和 `gc_object.c:722-749`；Vector2 的两个字段不沿用 Quaternion 第四字段 nodeMap 扩桶失败的证据。

**BUG:** `toString` 格式结果为未驻留短串且 short-intern 哈希表需要扩容时，HASH_BUCKET raw allocation 失败可使字符串创建返回 null 而没有 VM 异常；`SetString` 不写槽，helper 仍返回 true，dispatcher 交付预置 null。完整链见 `common.c:512-519`、`string.c:802-808,958-974`、`hash_set.h:525-537`、`hash_set.c:170-181`、`memory.h:19-22`（raw allocator 原样返回，不设置 VM 异常）及 `native_binding_dispatch.c:362,1303-1316`。该触发条件不等同于所有内存不足，也未做故障注入。

**TODO:** 对照 `math_common.h:20-23,54-55` 核查将 Vector2 struct 地址当作连续 `double` 数组的对象模型与布局前提；依赖不止 `dot`，还包含 length、lengthSquared、normalized 和 compare。compare 当前按平方长度返回 -1/0/1；NaN 或双方平方溢出使两个严格比较均为假而落到 0，其 unordered 策略须结合 VM compare 消费规则和具体边界断言继续核实。

`tests/fixtures/projects/native_numeric_pipeline/src/signal.zr:8` 确实构造并归一化 Vector2，但下游 checksum 只消费复数 magnitude；现有项目断言不验证返回的 vx/vy。provider 测试核查模块注册契约，不构成 Vector2 数值、字段 pin OOM 或短串分配失败的执行证据。完整逐单元审查与结论见 [Vector2 台账](../../code-review/coverage/zr_vm_lib_math_vector2.tsv)。
