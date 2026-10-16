---
related_code:
  - zr_vm_lib_math/include/zr_vm_lib_math/math_common.h
implementation_files:
  - zr_vm_lib_math/src/zr_vm_lib_math/common.c
plan_sources:
  - user: 2026-10-16 active BUG repair, Math integer and Tensor input boundaries
tests:
  - tests/library/test_math_input_boundaries.c
  - tests/cmake/math-input-boundary-tests.cmake
---

# Math 整数转换与 Tensor 输入边界

`ZrMath_IntFromValue` 是 Math 的共用整数读取入口，供数组 shape/index 以及 Tensor
公开 rank/size 字段使用。Library 的 `ZrLib_CallContext_ReadInt` 是独立实现，本次未改动。

## 转换契约

保留整数标签的既有处理与浮点朝零截断策略。浮点转换先检查有限值，并要求
`-2^63 <= value < 2^63`，再执行 C 的浮点转整数。下界使用可被 double 精确表示的
INT64_MIN，取其相反数得到独占上界，避免 double(INT64_MAX) 舍入到 +2^63 后误放行。
NaN、正负无穷、范围外有限值返回 false，不修改输出槽。普通非数值标签也保持 false。
超范围 UINT64 转为 Int64 的既有实现定义行为仍属独立契约问题，本次未更改。

该边界原则与本地参考 `lua/src/luaconf.h` 的 lua_numbertointeger 一致：在原生转换前
用包含下界、排除上界的比较证明可表示性。Math 已有头文件明确允许小数截断，因此
本修复没有引入“必须整值”的新规则。

## Tensor 输入校验

TensorTotalSize 对每个维度先调用共用整数转换，再检查正数。已知 total、dimension 均为
正数后，先判断 `total > INT64_MAX / dimension`，失败返回原接口的 -1 哨兵，通过才相乘。
空 shape 的既有乘积 1 保持不变；正常正小数维度仍按截断值计算。

TensorPopulate 先核对该乘积与 data 数量，再通过 ArrayReadFloat 检查每个 data 元素的
数值标签。任何非法元素均在写入 shape、__data、rank、size 字段之前返回 false。
这一步不改变数值算法，也不要求数值载荷必须有限：NaN/Inf 仍属于数值 data。
这些共用检查由构造与工厂路径继承，不改动 dispatcher 的失败语义。

## 回归证据

根代理使用当前 GC ABI 重新编译实际 Core/Library/Math 后取得真实 RED，收据为
`reports/review-bugfix-20261016/integrated-support-batch-3-red-receipt.json`，输入
changed_inputs=[]。9 项中 int_controls、tensor_controls 通过；另外 7 个边界用例失败。
失败后才写入本次实现；修复后的 support-batch-5-green 实际重编译成功，九项全部通过，1929 个输入哈希无变化。

测试的转换层直接验证 ±1e300、NaN、±Inf、-2^63、+2^63、上界内最后一个 double、
下界外相邻 double、输出槽保持及正常小数截断；不在测试自身执行危险 cast 或乘法。
真实 VM 数组覆盖 ArrayReadInt、shape [INT64_MAX, 2]、string/bool/null data，以及
rank/size 字段转换失败。正常 Tensor 使用小数 shape [2.9, 1.9] 与混合整数/浮点 data，
断言存储字段和形状兼容性。各例独立进程，Windows 禁止故障对话框，超时 20 秒。

VM 夹具暂停自动 GC，以便短时借用宿主指针；所有实际分配类别、尺寸和释放通过 tracker
核对，销毁后必须零残留。回归直接覆盖支持层，未额外运行 public constructor/dispatcher
端到端测试，也未对 Tensor 字段写入 OOM、可变 shape 与缓存不一致等独立问题作结论。
