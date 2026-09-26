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

`Complex` 字段 `real`、`imag`；方法 `magnitude`、`phase`、`conjugate`、`normalized`。
乘法公式为 `(a+bi)(c+di)=(ac-bd)+(ad+bc)i`，compare 按 magnitude squared。

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
