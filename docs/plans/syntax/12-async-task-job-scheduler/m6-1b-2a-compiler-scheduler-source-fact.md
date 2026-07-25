# Syntax 12 M6.1b.2a: Compiler Scheduler Source Fact

## 状态与产出记录

- 状态：`completed`
- 开始时间：2026-07-25 16:24 +08:00
- 完成时间：2026-07-25 17:19 +08:00
- 完成项目：
  - 在 `SZrFunction` 上发布并释放 compiler-owned
    `SZrFunctionSchedulerSourceFact`；它仅保留 canonical scheduler `TypeId`、
    resolved schedule member/signature token、signature hash、TaskScheduler
    protocol mask 与 `TASK_SCHEDULER_SCHEDULE` contract role。
  - 在 resolved receiver-call 路径中只于 exact contract role、protocol 与
    member identity 均可用时追加事实；同结构事实合并，非 scheduler call 或
    identity 缺失保持 unavailable。
  - 为 native descriptor 类型方法接入既有 deterministic metadata identity
    generator，使 `zr.thread.ThreadScheduler.schedule` 可提供精确 member 与
    signature identity，而非从名称或显示文本推断。
  - 以真实 source 编译覆盖单次 scheduler call、重复 call 去重、无 scheduler
    call 无事实；没有添加 artifact writer，也没有把 legacy `.zrb` 或手工
    fixture 当作 producer。

## 验证

| Toolchain | `zr_vm_artifact_schema_test` | `zr_vm_canonical_consumers_test` |
|---|---|---|
| GCC 11.4 | 18/18，真实 exit 0 | 17/17，真实 exit 0 |
| Clang 14.0 | 18/18，真实 exit 0 | 17/17，真实 exit 0 |
| MSVC 19.44 | 18/18，真实 exit 0 | 17/17，真实 exit 0 |

RED 先以新增 API/字段缺失的编译契约失败固定底层需求；首版 publisher
编译后真实 source case 仍得到 `schedulerSourceFactLength == 0`，定位为 native
descriptor 类型方法尚未投影 metadata identity。接入既有 deterministic identity
生成器后，三工具链均通过上述 focused tests。

## 后续边界

M6.1b.2b 必须先把本事实关联至 exact local `TypeDef` 或 imported `TypeRef`，
才允许写入 `.zri`/`.zro` artifact。缺少该 join 时不得创造 token、不得按
provider 名称或 runtime value 类别回退，也不得将 legacy `.zrb` stream 描述为
source-produced artifact。
