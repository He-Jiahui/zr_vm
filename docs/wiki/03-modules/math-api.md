---
related_code:
  - zr_vm_lib_math/include/zr_vm_lib_math/module.h
  - zr_vm_lib_math/include/zr_vm_lib_math/scalar.h
  - zr_vm_lib_math/include/zr_vm_lib_math/vector2.h
  - zr_vm_lib_math/include/zr_vm_lib_math/vector3.h
  - zr_vm_lib_math/include/zr_vm_lib_math/vector4.h
  - zr_vm_lib_math/include/zr_vm_lib_math/complex.h
  - zr_vm_lib_math/include/zr_vm_lib_math/quaternion.h
  - zr_vm_lib_math/include/zr_vm_lib_math/matrix3x3.h
  - zr_vm_lib_math/include/zr_vm_lib_math/matrix4x4.h
  - zr_vm_lib_math/include/zr_vm_lib_math/tensor.h
  - zr_vm_lib_math/src/zr_vm_lib_math
implementation_files:
  - zr_vm_lib_math/src/zr_vm_lib_math/scalar/scalar.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector2/vector2.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector3/vector3.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector4/vector4.c
  - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c
  - zr_vm_lib_math/src/zr_vm_lib_math/quaternion/quaternion.c
  - zr_vm_lib_math/src/zr_vm_lib_math/matrix3x3/matrix3x3.c
  - zr_vm_lib_math/src/zr_vm_lib_math/matrix4x4/matrix4x4.c
  - zr_vm_lib_math/src/zr_vm_lib_math/tensor/tensor.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/library-and-builtins/index.md
tests:
  - tests/math/test_math_module.c
  - tests/math/test_tensor.c
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
| `clamp` | `clamp(value, lower, upper)`，先保证边界顺序，再截断到区间。 |
| `lerp` | `lerp(a,b,t) = a + (b-a)*t`。 |
| `sqrt` / `rsqrt` | `sqrt(x)`；`rsqrt(x)=1/sqrt(x)`。 |
| `pow` / `exp` / `log` | 委托 C math；域错误按 provider 异常/NaN 规则处理。 |
| `sin`、`cos`、`tan` | 弧度制三角函数。 |
| `asin`、`acos`、`atan`、`atan2` | 反三角函数；`atan2(y,x)` 保留象限。 |
| `floor`、`ceil`、`round`、`sign` | 舍入和符号。 |
| `degrees` / `radians` | 角度和弧度转换。 |
| `almostEqual` | `almostEqual(lhs,rhs,epsilon?:float): bool`，缺省使用 EPSILON。 |
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
| `compare` | 比较平方长度，避免不必要开方。 |
| `toString` | 格式化为调试文本。 |

`Vector3` 额外提供 `cross(other)`。构造器按字段顺序接收分量；meta constructor 允许零值
或完整分量，具体 arity 由 descriptor 检查。

## Complex 和 Quaternion

`Complex` 字段 `real`、`imag`；方法 `magnitude`、`phase`、`conjugate`、`normalized`。
乘法公式为 `(a+bi)(c+di)=(ac-bd)+(ad+bc)i`，compare 按 magnitude squared。

`Quaternion` 字段 `x/y/z/w`；方法 `length`、`lengthSquared`、`normalized`、`conjugate`、
`inverse`、`dot`、`mul`、`slerp`。接近零长度时 inverse/normalized 采用零值保护；slerp
对 dot 进行夹紧并处理近线性情形，避免 `acos` 域误差。

```zr
let q0 = init math.Quaternion(0.0, 0.0, 0.0, 1.0);
let q1 = init math.Quaternion.fromAxisAngle(axis, math.radians(90.0));
let halfway = q0.slerp(q1, 0.5);
```

上例中的 `fromAxisAngle` 仅在构建配置提供扩展 descriptor 时可用；核心 descriptor 的稳定
surface 是 constructor、算术 meta 和列出的实例方法，遇到未注册成员应以 reflection 查证。

## Matrix3x3/4x4

矩阵字段按 row-major 暴露：`m00 ... m22` 或 `m00 ... m33`。两者都提供
`identity`、`transpose`、`determinant`、`inverse`、`mulVector`、`mulMatrix` 和 meta `mul`/
`toString`。Matrix4x4 还提供静态 `translation(x,y,z)`、`scale(x,y,z)`、`rotationX/Y/Z(angle)`。

```zr
let model = math.Matrix4x4.translation(1.0, 2.0, 0.0)
           .mul(math.Matrix4x4.rotationZ(math.radians(45.0)));
let world = model.mulVector(position);
```

determinant 接近零时 inverse 失败或返回 provider 定义的零/错误结果；需要可恢复行为时先
检查 determinant，再调用 inverse。矩阵乘法的顺序不可交换，文档示例按实现的 row-major
约定解释。

## Tensor

`Tensor` 字段：`shape: array`、`rank: int`、`size: int`。构造器 descriptor 形状为
`Tensor(shape: array, fillValue: float)`；实现会读取 shape 和数据/填充值并建立 row-major
storage。为避免版本差异，建议通过 `shape`、`fill` 和 `set` 明确初始化：

```zr
let tensor = init math.Tensor([2, 3], 0.0);
tensor.set([0, 1], 4.0);
let value = tensor.get([0, 1]);
let transposed = tensor.transpose2D();
let product = tensor.matmul(other);
```

| 方法 | 规则 |
| --- | --- |
| `clone` | 深复制 shape 和数据。 |
| `reshape(shape)` | 新 shape 的元素总数必须与 size 相同；不复制数据。 |
| `fill(value)` | 覆盖全部元素。 |
| `get(indices)` / `set(indices,value)` | indices 长度必须等于 rank；按 row-major 计算 offset。 |
| `sum` / `mean` | 全量聚合；空 tensor 的 mean 抛/返回 provider 定义错误。 |
| `transpose2D` | 仅 rank=2；返回转置副本或 view（以 descriptor 文档为准）。 |
| `matmul` | 两个 rank=2，左列数等于右行数。 |
| `add` / `sub` | shape 完全相同。 |
| `mulScalar` | 每元素乘一个 scalar。 |
| `toArray` | 返回数据副本，不暴露内部 storage。 |

所有 shape、indices 和矩阵维度错误在 runtime 检查；不要用负 index 依赖 C 数组下溢。

## 计算和对象分配

向量/矩阵/complex/quaternion 的 meta 运算返回新 inline/boxed value，compiler 可在 AOT 中
消除临时分配；Tensor 操作通常分配新 managed object。调用方若在 tight loop 中使用 Tensor，
可通过 `reshape`/`fill` 重用对象，但仍需注意 GC safepoint。

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
