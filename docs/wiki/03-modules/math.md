---
related_code:
  - zr_vm_lib_math/include/zr_vm_lib_math/module.h
  - zr_vm_lib_math/src/zr_vm_lib_math/module.c
  - zr_vm_lib_math/src/zr_vm_lib_math/scalar/scalar.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector3.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector4.c
  - zr_vm_lib_math/src/zr_vm_lib_math/matrix/matrix3x3.c
  - zr_vm_lib_math/src/zr_vm_lib_math/matrix/matrix4x4.c
  - zr_vm_lib_math/src/zr_vm_lib_math/quaternion/quaternion.c
  - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c
  - zr_vm_lib_math/src/zr_vm_lib_math/tensor/tensor.c
  - zr_vm_lib_math/src/zr_vm_lib_math/tensor/tensor_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/common.c
implementation_files:
  - zr_vm_lib_math/src/zr_vm_lib_math/module.c
  - zr_vm_lib_math/src/zr_vm_lib_math/scalar/scalar.c
  - zr_vm_lib_math/src/zr_vm_lib_math/tensor/tensor.c
  - zr_vm_lib_math/src/zr_vm_lib_math/tensor/tensor_registry.c
  - zr_vm_lib_math/src/zr_vm_lib_math/common.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - user: 2026-09-26 全仓库首方代码调用链审查与注释任务
  - docs/plans/syntax/README.md
tests:
  - tests/fixtures/projects/native_math_export_probe/native_math_export_probe.zrp
  - tests/fixtures/projects/native_numeric_pipeline/native_numeric_pipeline.zrp
  - tests/fixtures/projects/native_numeric_pipeline/src/tensor_pipeline.zr
  - tests/library/test_official_provider_convergence.c
doc_type: module-detail
---

# `zr.math`

**状态：`current`；Runtime provider，descriptor 版本 `1.0.0`。模块导出常量、标量函数和
八个数值类型；AOT/VM 使用相同的 descriptor 顺序和布局。**

`zr.math` 是 Runtime 数值 provider。所有导出类型均由 descriptor 注册，类型布局和
operator meta method 由 parser 投影到 canonical type graph；AOT 使用同一布局，不复制一份
数学类型表。

## 导出面

导出面分为模块级常量与标量函数、向量/四元数/复数类型、矩阵类型和张量类型；下表按
descriptor 的注册顺序给出可直接查阅的调用形状。

## 模块函数与常量

常量为 `PI`、`TAU`、`E`、`EPSILON`、`INF` 和 `NAN`，类型均为 `float`。标量函数的
descriptor 签名如下；所有参数按值传递，除 `almostEqual` 外返回 `float`：

| 函数 | 签名 | 说明 |
| --- | --- | --- |
| `abs` | `abs(value: float): float` | 绝对值 |
| `min` / `max` | `min(lhs: float, rhs: float): float`；`max(lhs: float, rhs: float): float` | 两值比较 |
| `clamp` | `clamp(value: float, low: float, high: float): float` | 约束到闭区间 |
| `lerp` | `lerp(a: float, b: float, t: float): float` | 线性插值 |
| `sqrt` / `rsqrt` | `sqrt(value: float): float`；`rsqrt(value: float): float` | 平方根/倒平方根 |
| `pow` | `pow(base: float, exponent: float): float` | 幂 |
| `exp` / `log` | `exp(value: float): float`；`log(value: float): float` | 指数/自然对数 |
| `sin` / `cos` / `tan` | 各接收一个 `float`，返回 `float` | 三角函数 |
| `asin` / `acos` / `atan` | 各接收一个 `float`，返回 `float` | 反三角函数 |
| `atan2` | `atan2(y: float, x: float): float` | 保留象限的反正切 |
| `floor` / `ceil` / `round` | 各接收一个 `float`，返回 `float` | 舍入 |
| `sign` | `sign(value: float): float` | 符号值 |
| `degrees` / `radians` | 各接收一个 `float`，返回 `float` | 角度转换 |
| `almostEqual` | `almostEqual(lhs: float, rhs: float, epsilon?: float): bool` | 仅比较绝对误差 `abs(lhs-rhs) <= epsilon` |
| `invokeCallback` | `invokeCallback(callback: function, value: float): float` | 通过 `ZrLibCallContext` 调用 ZR callable |

值类型：`Vector2`、`Vector3`、`Vector4`、`Quaternion`、`Complex`、`Matrix3x3`、
`Matrix4x4`；引用类型：`Tensor`。

向量提供分量、长度、归一化、点积和插值，`Vector3` 另提供叉积；矩阵提供转置、乘法、
行列式和逆，逆矩阵计算在实现判定不可逆时返回 native 失败。`Quaternion` 提供共轭、
归一化、乘法和球面插值；`Complex` 提供实部/虚部、共轭和复乘。`Tensor` 用公开的
`shape/rank/size` 字段与隐藏的 `__data` 数组保存 row-major 多维数据。

## 类型成员清单

下表列出 descriptor 中的命名方法；各类型的运算符支持范围以其 descriptor 的 meta
method 为准。除 Tensor 的 `fill/set` 会修改 receiver 并返回它自身外，表中返回对象的
方法创建新对象。

| 类型 | 构造器与命名方法 |
| --- | --- |
| `Vector2(x, y)` | `length(): float`、`lengthSquared(): float`、`normalized(): Vector2`、`dot(other: Vector2): float`、`distance(other: Vector2): float`、`lerp(other: Vector2, t: float): Vector2` |
| `Vector3(x, y, z)` | `length()`、`lengthSquared()`、`normalized()`、`dot(other: Vector3)`、`distance(other: Vector3)`、`lerp(other: Vector3, t: float)`、`cross(other: Vector3): Vector3` |
| `Vector4(x, y, z, w)` | `length()`、`lengthSquared()`、`normalized()`、`dot(other: Vector4)`、`distance(other: Vector4)`、`lerp(other: Vector4, t: float)` |
| `Quaternion(x, y, z, w)` | `length(): float`、`lengthSquared(): float`、`normalized(): Quaternion`、`conjugate(): Quaternion`、`inverse(): Quaternion`、`dot(other: Quaternion): float`、`mul(other: Quaternion): Quaternion`、`slerp(other: Quaternion, t: float): Quaternion` |
| `Complex(real, imag)` | `magnitude(): float`、`phase(): float`、`conjugate(): Complex`、`normalized(): Complex` |
| `Matrix3x3(...values)` | `identity(): Matrix3x3`（静态）；`transpose(): Matrix3x3`；`determinant(): float`；`inverse(): Matrix3x3`；`mulVector(v: Vector3): Vector3`；`mulMatrix(m: Matrix3x3): Matrix3x3` |
| `Matrix4x4(...values)` | `translation(x, y, z)`、`scale(x, y, z)`、`rotationX/Y/Z(angle: float)`、`identity()`（均静态）；`transpose()`、`determinant()`、`inverse()`、`mulVector(v: Vector4)`、`mulMatrix(m: Matrix4x4)` |
| `Tensor(shape: array, data: array)` | `clone()`、`reshape(shape: array)`、`fill(value: float)`、`get(indices: array)`、`set(indices: array, value: float)`、`sum()`、`mean()`、`transpose2D()`、`matmul(other: Tensor)`、`add/sub(other: Tensor)`、`mulScalar(value: float)`、`toArray()` |

`Tensor` 构造回调实际读取 shape 与 data 两个数组，要求 data 长度等于各维度之积；当前
descriptor 的签名文字仍误写为 `fillValue: float`。`get/set` 的单个参数是长度与 rank 相同
的多维 indices 数组，按 row-major 映射到 data；`set` 原位更新并返回 receiver。
矩阵构造器支持零参数（生成 identity）或恰好 9/16 个 `float` 参数。

## 数值规则

浮点运算遵循宿主 IEEE-754；不会把 `-0` 规范化成 `0`，除非具体函数文档明确要求。
整数转换在 parser 阶段记录 checked/unchecked 事实：超出目标范围的 checked cast 抛
`TypeError`，unchecked 路径按目标宽度截断。跨 VM/AOT 的比较、NaN 和溢出行为由 shared
helper 保持一致。

## 类型和调用契约

模块初始化按 scalar、vector、quaternion、complex、matrix、tensor registry 顺序收集
function/type descriptors，并发布 `zr.math`、版本 `1.0.0`、native plugin ABI 和
runtime ABI。每个 descriptor 的泛型参数和字段偏移在注册时验证；`invokeCallback` 通过
`ZrLibCallContext` 调用 ZR callable，异常会恢复调用栈后再返回失败。

## C API

```c
const ZrLibModuleDescriptor *ZrVmLibMath_GetModuleDescriptor(void);
TZrBool ZrVmLibMath_Register(SZrGlobalState *global);
```

`GetModuleDescriptor` 返回进程内静态 descriptor，不应由宿主释放。`Register` 失败通常
表示 ABI/contract 冲突或 global 已关闭；错误细节写入 global diagnostic。共享构建导出
`ZrVm_GetNativeModule_v1()`。数学模块不拥有调用方传入的 `SZrTypeValue`，native callback
返回值必须写入由 binding 分配的 result slot。
