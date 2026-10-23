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
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
  - zr_vm_lib_math/include/zr_vm_lib_math/matrix3x3_registry.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_core/src/zr_vm_core/string/string.c
  - zr_vm_core/include/zr_vm_core/conversion.h
  - zr_vm_core/include/zr_vm_core/string.h
  - zr_vm_common/include/zr_vm_common/zr_string_conf.h
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/base/memory.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_parser/src/zr_vm_parser/syntax/lexer.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_lib_math/CMakeLists.txt
  - zr_vm_common/CommonMacros.cmake
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
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - user: 2026-09-26 全仓库首方代码调用链审查与注释任务
  - docs/library-and-builtins/index.md
tests:
  - tests/fixtures/projects/native_numeric_pipeline/src/tensor_pipeline.zr
  - tests/fixtures/projects/native_numeric_pipeline/src/signal.zr
  - tests/fixtures/projects/native_math_export_probe/src/main.zr
  - tests/library/test_official_provider_convergence.c
  - tests/module/test_module_system.c
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

compare 按模长平方返回 -1/0/1，不比较分量字典序。**BUG:** 有限 (1e200, 0) 与 (2e200, 0) 的平方模长都溢出到 Inf，当前返回 0，折叠了不同模长。
**TODO:** NaN / unordered 的 VM tri-state 策略仍待结合实际 compare 消费规则核查；不把这一政策疑问与上述有限输入缺陷混同。
**BUG:** 有限 `(1e200, 0)` 的平方中间值溢出，使 magnitude 得到无穷、normalized 得到
零复数。这是当前源码可证实的数值限制，本次未新增运行复现。**BUG:** 两个有限 (1e200, 1e200) 相乘，精确实部为 0，当前两个中间乘积溢出后以 Inf-Inf 写入 NaN。

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
[Quaternion 审查台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_math_quaternion.tsv)。该结论是
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
交换，文档示例按实现的 row-major 约定解释。Matrix3x3 的回调在 `abs(det) <= 1e-9` 时返回
false；若 VM 没有异常，native dispatcher 将该失败投影为 null。descriptor 的返回类型文本仍为
`Matrix3x3`，调用方需要处理此失败边界；阈值策略与类型投影的对应关系仍待核查。

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
| `clone` | 浅复制shape/data外层容器并返回新Tensor；不递归复制元素对象。 |
| `reshape(shape)` | 新 shape 的元素总数必须与 size 相同；返回 shape 和 data 均复制的新 Tensor。 |
| `fill(value)` | 尝试原位覆盖data元素并返回receiver；void setter不报告逐项写入状态。 |
| `get(indices)` / `set(indices,value)` | indices长度须等于实际shape数组长度；按row-major计算offset；set尝试写入并返回receiver。 |
| `sum` / `mean` | 累加可读取的数值项；mean除以缓存size且要求大于零；非数值项被跳过的缺陷见下文。 |
| `transpose2D` | 仅 rank=2；返回数据重排后的新 Tensor。 |
| `matmul` | 两个 rank=2，左列数等于右行数。 |
| `add` / `sub` | ShapeEquals比较维度个数及转成int64后的逐维值；小数维度政策待核。 |
| `mulScalar` | 每元素乘一个 scalar。 |
| `toArray` | 返回data外层数组的浅副本；copy helper忽略append软失败的限制见下文。 |

实现逐项拒绝非正维度并检查普通尺寸匹配、indices 数量和索引范围；shape乘积未检查有符号溢出。
BUG: shape/index 中有限 1e300 可沿 common.c:356 到 :156 触发浮点转 int64 的 C 未定义行为；两个 3037000500 维度可在 :380 触发有符号乘法溢出。
可表示浮点维度/索引的小数部分当前被截去，是否拒绝小数仍是 TODO；调用者应传可表示的整数维度/索引和数值数据。

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
**BUG:** 新对象 factory 在合法定制 allocator 仅拒绝首次 ignored-root ARRAY 扩容、其余步骤成功时，会忽略 receiver pin 失败并成功交付缺字段对象；注册时的 prototype 只有字段 descriptor，没有 real/imag 默认值。Construct 复用 target 的路径仍为 TODO，完整失败链见 [Complex 审查记录](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/batches/20261014-complex-contracts.md)。

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

**BUG:** Vector2 回调将快照地址传给 Dot(count=2)，在 AVX2 分支跳过四项向量循环后以 common.c:62 读取下标1，纯标量 :102 也越出首成员 double 对象（C11 N1570 §6.5.6p7–8）；SSE2/NEON 的单次 intrinsic load 对象范围仍 TODO，见 math_common.h:51、134、135。该结论未声称默认构建执行过边界输入。**TODO:** compare 对 NaN 或双方平方和溢出均落入0，其 unordered 策略须沿实际VM比较/排序消费者核查。

`tests/fixtures/projects/native_numeric_pipeline/src/signal.zr:8` 确实构造并归一化 Vector2，但下游 checksum 只消费复数 magnitude；现有项目断言不验证返回的 vx/vy。provider 测试核查模块注册契约，不构成 Vector2 数值、字段 pin OOM 或短串分配失败的执行证据。完整逐单元审查与结论见 [Vector2 台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_math_vector2.tsv)。


## Vector3 回调、结果与数值边界

Vector3 constructor 元数为 3，命名方法 length/lengthSquared/normalized 为 0、dot/distance/cross 为 1、lerp 为 2；meta add/sub/compare 为 1、neg/toString 为 0。native dispatcher 在回调前校验元数。ReadFloat 接受整数与浮点值，错误标签抛 TypeError；二元方法的 rhs 由 ReadObject 接受 object/array，再逐字段读取数值 x/y/z，不要求 rhs 的 prototype 恰为 Vector3。字段缺失或非数值正常 false，在 thread 状态仍 FINE 时会交付预置 null，不能把异常与此普通失败混为一类。

构造 helper 可以复用兼容 self，否则依据 construct target/owner prototype 新建实例；三维结果工厂另行创建 VM 对象。字段名表是静态存储，构造 values[3] 是本次调用的栈数组，索引顺序均为 x/y/z。context、state、receiver、rhs、result 只在同步回调期间借用；新对象/字符串由 VM 管理，不向调用者转移手动 free 责任。registry 的 type、字段/方法/meta 表与 hints 都借用 provider image，消费者存续期间须保持映像驻留。GetFunctions 返回 NULL 并将可选 count 写 0；它不表示实例方法或 meta callback 不存在。

**BUG:** 合法三数值构造入口经 CREATE_OBJECT/TO_STRUCT 和 fast lane 取得尚未显式 pin 的 seed receiver。ignored-root 表处于 capacity-1，receiver 与字段键此前不在表中时，setter 先 pin receiver 占最后一格，随后字段键的 ARRAY RawMalloc 扩容失败会让 void setter 静默跳过写入，ConstructFloatObject 仍完成并发布。Normalized/Lerp/Cross/MetaAdd/Sub/Neg 的 MakeVector3 工厂也逐字段调用 void setter，存在同类缺字段成功返回。三字段普通 nodeMap 容量 4、负载阈值 3，第三项恰好到阈值，不触发 Quaternion 第四项的 HASH_BUCKET 扩桶路径；本批不把那条独立缺陷移植到 Vector3。

**BUG:** 有限 `(1e200,0,0)` 的平方和中间溢出，Length 返回 Inf，Normalized 返回零向量，到零点的 Distance 返回 Inf，而相应长度与单位向量本可表示。Dot 的有限 `(1e200,1e200,0)` 与 `(1e200,-1e200,0)` 乘积先成为正负 Inf，累加为 NaN，数学结果为零。Cross 的 `(0,1e200,1e200)` 与自身运算产生 Inf-Inf，数学结果为零向量。Lerp 的 x 端点 `1e308/-1e308`、factor `.5` 先使差分溢出而返回 -Inf，数学中点为零；factor 本身不夹紧。上述均由合法参数与表达式闭合静态证据，未执行极值用例。LengthSquared 的平方值本身不可表示与这些可表示结果的中间溢出分开处理。

**BUG:** Vector3 的 Length、LengthSquared、Normalized、Dot 和 MetaCompare 传快照地址和count=3，标量与AVX2/SSE2/NEON尾项均读取下标2，越出首成员标量对象；common.c:62、79、94、102 与 math_common.h:134 按 C11 规则闭合证据。Distance使用真正的三元素数组，不能归入该cast。**TODO:** MetaCompare在NaN/双方Inf时写0的排序语义与消费规则仍待核，不能用值0推断对象相等。

**BUG:** toString 先在 512 字节栈窗口格式化，VM 字符串创建不保留该窗口。仅当格式文本尚未驻留且属于短串、插入触发 HASH_BUCKET rehash 而宿主 allocator 扩容失败时，String_Create 可不抛异常地返回 NULL，SetStringObject 不写结果槽，MakeStringResult 仍返回 true；dispatcher 最后交付预置 null。该具体分支和对象创建时抛出的硬失败分别记录，没有 allocator 故障注入信用。

现有 `test_native_vector3_constructor_binds_all_numeric_arguments_at_runtime` 明确断言 x/y/z 为 2/3/4，Root 的修改前整 module target 78 项运行中该项 PASS，另两项既有失败保留。pipeline 的 lin_alg 执行 dot/cross/cross.length，最终 checksum 只消费 dot，未消费 crossLength，不能为叉积或长度结果授予断言信用。极值、比较、布局和分配失败没有专门执行证据。完整定义、间接调用与测试边界见 [Vector3 台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_math_vector3.tsv) 与 [Vector3 批次](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/batches/20261014-vector3-contracts.md)。

最终 GC helper 文档补齐后的共享 target 验证另见 `gc-vector3-final-r3-results.json`：构建成功，78 项为 70 PASS / 8 FAIL；Vector3 构造、GC 控制及统计字段用例仍 PASS。原有两项失败保留，另六项二进制元数据往返用例由 PASS 变为 FAIL，尚未隔离归因。与旧 before 的输入已有九个并发 Parser 文件差异；本轮 1652 个输入在构建和测试期间稳定，但不能据此声称与旧基线完全一致或整个 suite 通过。Vector3 四源与 r2 冻结版本相同，严格非注释 token 证明和 r2 的四对象实际重编证据继续限定于其原范围。


## Vector4 回调、四字段发布与数值边界

Vector4 constructor 元数为 4；length/lengthSquared/normalized 为 0，dot/distance 为 1，lerp 为 2；meta add/sub/compare 为 1、neg/toString 为 0。descriptor 保存固定元数，native dispatcher 在每次回调前检查。ReadFloat 接受 VM 整数和浮点，错误标签抛 TypeError；rhs 由 ReadObject 接受 object/array，再按 x/y/z/w 读取数值字段，不要求名义 prototype 为 Vector4。字段缺失或非数值返回普通 false；thread 仍 FINE 时交付预置 null，异常路径另行退出。

构造 helper 可复用兼容 self，也可根据 construct target/owner prototype 新建实例；MakeVector4 运算工厂总是另建 VM 对象。静态字段名表与本次调用的 values[4] 均按 x/y/z/w 同步读取。context、state、receiver、rhs 和 result 借用于本次回调；对象及字符串由 VM 管理，调用者不获得手动释放责任。registry 的 type、字段/methods/meta 和 hints 表及其字符串均借用 provider image，消费者使用期间必须保持映像有效。GetFunctions 返回 NULL、可选 count 写 0，表示 module-level function 空表；实例方法和 meta callback 仍由 type descriptor 提供。

**BUG:** 普通四数值 constructor 经 CREATE_OBJECT/TO_STRUCT 取得空 nodeMap 的 seed，再由 FUNCTION_CALL 的数值 fast lane 和 ResolveConstructTarget 复用；MakeVector4 工厂也从空 nodeMap 新对象开始。初始容量 4、3/4 load threshold 下，前三字段成功写入后第四字段 w 触发 HASH_BUCKET rehash。若其他对象/字段键创建、所有 ignored-root pin 和 x/y/z 写入均成功，只有此次 `Memory_Allocate(HASH_BUCKET)` 返回 NULL，void setter 不反馈失败，构造 helper/factory 仍可成功发布缺 w 的对象。局部触发点见 `vector4.c:38-42,58-60`；下层链见 `common.c:22-35,200-252,267-270`、`execution_object.c:558-600`、`value_construct.c:185-190,330`、`hash_set.h:498-500`、`hash_set.c:170-181`、`memory.h:14-22`（raw allocator 不设置 VM 异常）、`object.c:1840-1847`（Add 失败仅记录日志并从 void setter 返回）和 `native_binding_dispatch.c:1406-1407`（调用 setter 后仍继续发布）。这是完整静态单故障路径，未做 allocator 故障注入。

**BUG:** ignored-root ARRAY 扩容是另一条字段缺失路径。seed/factory 新 receiver 和字段 key 尚未登记、表 count 为 capacity-1 时，Object_SetFieldCString 先 pin receiver 占最后一槽，再尝试 pin key；该次 `RawMallocWithType(ARRAY)` 失败可让 void setter 静默跳过写入，helper/factory 仍完成。此链见 `gc_object.c:722-749` 和 `native_binding_dispatch.c:210,1385-1407`；不能把 ARRAY 失败与上一段仅 w 的 HASH_BUCKET 失败合并成任意 OOM 结论。

**BUG:** 有限 `(1e200,0,0,0)` 的平方和中间溢出，使 Length 返回 Inf、Normalized 返回零向量，到零点的 Distance 返回 Inf，而相应长度和单位向量本可表示。Dot 的 `(1e200,1e200,0,0)` 与 `(1e200,-1e200,0,0)` 先生成 +Inf/-Inf，累加为 NaN，数学点积为零。Lerp 的 x 端点 `1e308/-1e308`、factor `.5` 先使差分溢出，数学中点为零却返回 -Inf；factor 不夹紧。LengthSquared 的平方值本身不可表示与这些有限结果的中间溢出分别记录。本批证据为合法输入和实际表达式的静态闭合，未执行极值样例。

**BUG:** Vector4 的相关回调传快照地址和count=4；SSE2/NEON第二轮在common.c:71、72、87、88计算首成员指针+2，纯标量:102也越界。**TODO:** AVX2单次intrinsic load的对象范围仍待核。Distance使用真正d[4]不属于cast问题；MetaCompare对NaN/双方平方为Inf落入0的比较策略仍须追踪VM消费者，无当前backend边界运行信用。

**BUG:** toString 使用 512 字节栈格式窗口，VM 字符串创建不保留该栈地址。调用链从 `common.c:512-519` 经 `Value_SetString`（`native_binding_dispatch.c:1303-1307`）进入 `native_binding_support.c:320-324` 的 native_binding_create_string，再到 `string.c:958-974` 的 CreateTryHitCache。仅当格式文本为未驻留短串，缓存 miss 后进入 Create/short interning，并且驻留表所需的 HASH_BUCKET rehash 宿主分配失败时，创建才不抛 VM 异常地返回 NULL；SetStringObject 不覆写结果槽，MakeStringResult 仍 true，成功派发交付预置 null。失败与回写证据见 `string.c:802-808`、`hash_set.h:525-537`、`hash_set.c:170-181`、`memory.h:14-22`（raw allocator 不设置 VM 异常）和 `native_binding_dispatch.c:362,1311-1316,673`。不泛化为所有分配失败，无故障注入执行信用。

Matrix4x4 的 mulVector/meta mul 是实际 C 消费者。pipeline 的 `lin_alg.zr:11-18` 确实构造 Vector4、执行矩阵乘向量并把 XYZ 放进记录，但 callbacks/probe 只把记录中的 dot 纳入 checksum，未数值断言这些 XYZ。因此该夹具不提供 Vector4 数值、比较、布局或字段/短串分配失败的断言证据，本批也不运行会改写夹具目录的 project suite。完整逐单元定义、绑定、调用与结论见 [Vector4 台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_math_vector4.tsv) 和 [Vector4 批次](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/batches/20261014-vector4-contracts.md)。

本批与 SystemVM 共用 `system-vm-vector4-final-r1-results.json` 的实际验证：四个实现对象强制重建、构建成功；模块测试 78 项为 70 PASS / 8 FAIL，全部 78 行与最近 r3 基线精确一致，provider 9 项全部 PASS。相对更早 76 PASS / 2 FAIL 的记录，六项二进制元数据失败仍保留，本批未新增失败。当前输入与 r3 有三个并发 core/Parser 文件差异；保守扩展的 2950 个输入在构建及两个 suite 期间稳定，八个冻结源码未变化。Parser 的两条 C4701 和其他配置测试的 CMAKE_OBJECT_PATH_MAX 警告另行记录，所选 target 构建成功；不据此授予 Vector4 极值、布局或分配失败测试信用，也不声称整个 suite 通过。

## Matrix3x3 调用与失败边界

Matrix3x3 是按 row-major 字段 `m00` 至 `m22` 存储的值类型；矩阵乘列向量，矩阵乘法为左乘右。
构造 callback 只接受零个参数（identity）或完整九个数值；descriptor 的 0..9 区间还会放行 1..8 个参数，
callback 随后拒绝，却仍报告“expected 0..9”，这是元数错误文本与实际接受条件不一致的 BUG。
`identity` 是静态新值工厂，其他命名运算产生新结果；构造 helper 则可填充解析器预建的兼容 receiver。
接收值与参数在同步 native 调用内借用，局部 C 数组不会被保存到回调之外。

**BUG:** 行列式与乘法使用直接浮点公式。取有限 `A=1e200`，矩阵 `[[A,A,0],[A,A,0],[0,0,1]]`
的数学行列式为零，中间乘积却溢出为 Inf，随后 Inf-Inf 得到 NaN；逆矩阵的绝对阈值检查也不会拒绝 NaN。
相同矩阵乘列向量 `[A,-A,0]`，以及乘首列为该向量的矩阵，有限乘积的抵消也可变成 NaN。
这些是当前公式的静态反例，未运行极值数值测试；不据现有 module/provider 用例宣称这些边界已验证。

**BUG:** 工厂和构造 helper 使用 void 字段 setter。对初始容量 4、阈值 3 的空字段表，写第 4 个字段时需要
扩桶；在默认 realloc 分配器或同样保留失败前旧块的分配器下，若单次 `HASH_BUCKET` 扩容软失败，
其他 pin、键和分配均成功且 VM 无异常，setter 只记录日志，callback 仍可交付缺字段矩阵。
新对象工厂另有 ignored-root `ARRAY` 扩容软失败遗漏字段的受限路径；toString 还存在短字符串缓存扩桶
软失败后返回 true 而结果保持 null 的路径。各路径的精确前提和调用证据分别记在台账，未做故障注入。

**TODO:** 长格式文本也经过共享 API 地址缓存。`MakeStringResult` 经 `Value_SetString`、
`native_binding_create_string` 进入 `CreateTryHitCache`；后者在比较缓存项时使用仅适用于短串内联字节的
`ZR_CAST_STRING_TO_NATIVE`。应沿 `native_binding_support.c:320-324`、`string/string.c:958-974`、
`conversion.h:73-76` 和 `string.h:35-37`，核对超过 127 字节的矩阵文本在后续同桶查询中的长串布局与可观察影响。
既存 Core 台账记录了该布局问题；本批没有长串同桶复测，不另授予 Matrix3x3 错误文本或无效读取的 BUG 结论。

本批没有找到 Matrix3x3 专项构造、数值或失败路径断言；现有 module/provider 目标链接 Math，
其 Vector3/sqrt 用例不能作为 Matrix3x3 数值覆盖。完整定义、绑定、间接入口和逐单元结论见
[Matrix3x3 台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_math_matrix3x3.tsv) 与
[审查批次](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/batches/20261014-matrix3x3-contracts.md)。

## Matrix4x4 调用与失败边界

Matrix4x4 以 row-major 字段 `m00` 至 `m33` 保存数值，矩阵乘列向量，`mulMatrix` 为左乘右。
`translation` 把平移量放在 `m03/m13/m23`；`rotationX/Y/Z` 接收弧度。构造只接受零参数
（identity）或完整十六个数值；descriptor 的 0..16 区间也放行 1..15 个参数，随后 callback
拒绝，却报告“expected 0..16”，其错误文本未说明实际接受的两种形状。静态工厂产生新值，
实例运算不修改输入；receiver 与参数只在同步 native 调用内借用。

**BUG:** 行列式使用绝对枢轴阈值 `1e-9`，`diag(1e-10,1e10,1,1)` 的数学行列式为 1，
当前分支却返回 0。对 `diag(1.5e154,1.5e154,2e-9,2e-9)`，每个枢轴都超过阈值，
数学行列式约为 `9e290`，累乘却先溢出为 Inf。矩阵乘向量和矩阵乘法也直接累加乘积：
取 `A=1e200`，行 `[A,A,0,0]` 与列 `[A,-A,0,0]` 的数学结果为 0，实际中间项可形成
Inf 与 -Inf 并得到 NaN。这些是完整公式路径的静态反例，本批没有执行极值数值测试。

**TODO:** `inverse` 在任一选定枢轴满足 `abs(pivot) <= 1e-9` 时返回 native false；
VM 无异常时 dispatcher 交付 null，虽然 descriptor 声明 Matrix4x4。`diag(1e-10,1e-10,1e-10,1e-10)`
可逆且条件数为 1，当前仍被拒绝；需确认这个绝对阈值与返回类型的语言契约。NaN 不会满足
该比较，返回成功也不保证输出有限或精确。未将阈值政策疑点升级为已证实的 inverse 缺陷。

**BUG:** 构造和工厂依赖 void 字段 setter。空字段表按容量 4/8/16 与阈值 3/6/12 扩容；
写入第 4、7、13 个字段时，若单次 `HASH_BUCKET` 扩容软失败，其他分配与 pin 成功且 VM 无异常，
默认 realloc 分配器保留旧块，callback 仍可交付缺字段对象。新工厂还存在 ignored-root `ARRAY`
扩容软失败的受限路径。短格式 `toString` 在未驻留短串的缓存扩桶软失败后，可返回 true 而结果
仍为 null。精确前提与共享 helper 调用证据记于台账，未做故障注入；长串 API 地址缓存的影响仍为 TODO。

现有工程样例包含 Matrix4x4 的 scale 数值断言与矩阵流水线入口，但本批未运行会重建产物目录的
工程样例 suite。module/provider 测试链接 Math，其执行结果不授予 Matrix4x4 数值、阈值、故障或 GC
专项覆盖。完整单元、注册项、调用入口与证据见
[Matrix4x4 台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_math_matrix4x4.tsv) 和
[审查批次](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/batches/20261014-matrix4x4-contracts.md)。


## 模块描述符与宿主注册边界（Module registration）

Math 入口只聚合 registry 并交给 native binding，不执行数值 callback。当前逐 getter 核对为 25 个模块函数、8 个类型、13 条结构化提示；类型方法仍在各自 type descriptor 中。三组静态容量为 48/8/32，类型表恰满。六个常量是 PI、TAU、E、EPSILON、INF、NAN。精简 JSON 仅列两个函数和八个类型，不能用它代替完整函数表；已核消费者是提示导出和 native 反射。

GetModuleDescriptor 惰性初始化并借出同一静态描述符，不注册到 VM。Register 初始化后把描述符借给指定 global 的 NativeRegistry；false 不保证 VM 没有变化，下层 Attach 可能已完成，静态描述符也不会因一次 VM 注册失败而回滚。描述符、嵌套表、文本及 callback 的有效期受模块映像限制，借用者不释放这些静态对象；共享库不能在 VM 或其他借用者仍使用它们时卸载。

**TODO:** 三个 append 没有容量检查；新增 registry 前须重新核对各 getter 数量及三组上界。g_math_initialized 是非原子完成标志；宿主须串行首次初始化，并在跨线程读取前建立同步，同一 global 的注册也由宿主协调。当前可检索入口没有证明允许并发首次调用，本批不把该策略疑点宣称为已复现数据竞争。

共享发现符号 ZrVm_GetNativeModule_v1 只在共享配置编译，由加载器按字符串查符号后经函数指针调用；它借出描述符，不自行注册或管理库句柄。加载器随后核对 ABI 与请求模块名。静态 Debug 构建不提供共享发现分支的执行信用。

完整定义、41 处实际 checkout 直接 Register 调用、间接插件入口、所有权和测试边界见 [模块入口台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_math_module.tsv) 与 [批次记录](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/batches/20261015-math-module-contracts.md)。现有 sqrt 转发测试只断言非空结果与标签；provider 的官方模块清单不等于三组计数或插件加载断言。

## Math 公共辅助层与 Tensor 存储边界

公共helper只在当前同步VM调用中借用state、receiver、数组和结果槽；按值快照不拥有VM资源。直接参数、字段和元素读取失败的普通false与VM非局部异常分别处理。WriteFloatField的直接消费者为固定分量构造器和七个对象工厂；Tensor使用WriteIntField发布rank/size。返回对象或true不能证明所有void字段写入成功。

**BUG:** Tensor的公开数值元素预期见tensor.h:12。Populate仅检查data长度，Tensor([1], ["x"])仍可建立，sum/mean跳过非数值项后继续返回数值。新Tensor空map容量4/阈值3，按shape、__data、rank、size写入，若仅第四项size的HASH_BUCKET扩容返回NULL，其余步骤成功，void setter静默漏字段而Populate仍true，构造或factory可交付随后GetStorage无法读取的实例。这是静态闭合的单次软失败链，未故障注入。

**TODO:** GetStorage在common.c:415还直接把公开rank/size转换为int64，只检查字段存在和标签，不重建shape/data/rank/size一致性；公开写入范围和缓存一致性政策须沿tensor_registry字段descriptor及VM成员写入规则核查，不能直接把所有修改输入都视为已证缺陷。

MakeStringResult使用固定512字节格式窗口；过长输出截断及短串缓存软失败true/null是独立边界。长串缓存布局仍见共享TODO。最终118个公共辅助层单元及每条BUG/TODO完整定义、调用、契约和验证边界见[公共层审查记录](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/batches/20261014-math-common-contracts.md)及[台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_math_common.tsv)。


## Scalar 数值与嵌套 callback 边界

Scalar 以25个模块函数 descriptor 进入 native派发，17个一元与4个二元实现由宏展开，另有 clamp/lerp/almostEqual/invokeCallback 四个显式实现。参数、context和result仅当前同步调用借用；ReadFloat接受整数/浮点并转换为double，缺参/错标签抛VM异常，不能笼统算作普通false。getter返回的只读静态表及函数地址仅在provider映像驻留时有效，调用者不释放或改写。

**BUG:** invokeCallback元数据声明float，但CallValue成功后原样转发callable的实际VM标签，例如bool或字符串。**TODO:** NativePointer作为callable的非空、ABI与代码映像有效期须沿宿主SetNativePointer入口核查。clamp倒置边界、lerp中间差溢出和almostEqual跨量级误差策略仍有明确核查入口；本批仅记录，不改策略。

现有module转发sqrt用例实际PASS，但只检查非空/标签。Rust sqrt数值断言、两个callback pipeline及LSP文档字符串入口本批均仅静态核查，后者不执行callback。94个唯一单元的调用、生命周期、无需另写注释的判断及验证限制见[Scalar台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_math_scalar.tsv)与[审查批次](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/batches/20261015-math-scalar-contracts.md)。


## Tensor 回调、失败与存储生命周期

构造、clone、reshape复制shape/data外层容器，元素对象仍共享。get/set按实际shape长度校验indices；rank/size是公开缓存，GetStorage不重建全部不变量。ReadArray/ReadObject/ReadFloat对错误参数标签抛VM类型异常；普通false与非局部异常分开。arity来自实际descriptor，constructorSignature文本不作类型门禁；Parser消费者只保存文本并检查无参构造形式，不按fillValue文本推断第二参数类型。

**BUG:** Populate仅核data长度，Tensor([1],["x"])可进入运算；Sum/Mean跳过失败读取而mean仍除以全部size，Transpose/Matmul/Add/Sub/MulScalar按局部零继续。各callback保留BUG结论，共享根因引用Populate.numeric-data-block。copy_array忽略PushValue普通false：唯一一次ignored-root ARRAY扩容返回NULL且其余步骤成功时，shape[1,2]可缩为[2]而总量匹配；ToArray可直接返回短data。Transpose输出首维为原cols=1、Matmul为左rows=1时，输出shape同类软失败可将rank2变为rank1。这些ARRAY路径与对象/短串HASH_BUCKET扩容分开，不概括任意OOM。MetaToString格式化短串未驻留且单次HASH_BUCKET扩容软失败时，可由MakeStringResult交付true/null。

**TODO:** Clone/Reshape的C实参求值顺序未规定，新数组可能跨另一复制和TensorMake分配。Construct中shapeCopy跨dataCopy分配，dataCopy还跨先写shape字段；两个显式root窗口需分别核查。Transpose/Matmul/binary/MulScalar的中间数组也跨后续分配。GcMalloc仅在首次raw申请失败、collector已初始化且非immediate-GC时进入full GC；PushValue仅在单次调用内pin/unpin，不能覆盖后续窗口。ToArray在copy返回后直接写result，不列入该疑点。未观察到回收、移动或故障执行。

Fill/Set的void写入状态、小数维度/索引向零截断与公开缓存一致性政策保留独立TODO，不改行为。人工numeric_pipeline含真实Tensor调用，但本批未运行project suite，toArray的arr不参与checksum214；两个module同名stub只返回11，parser/LSP匹配为静态文本。当前源通过3204tokens/8lexer与现有native构建，仍有原8项Module失败和Fs C1035，无Tensor数值或故障专项信用。逐单元证据见 [Tensor台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_math_tensor.tsv) 与 [批次](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/batches/20261015-math-tensor-contracts.md)。
