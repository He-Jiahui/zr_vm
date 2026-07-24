# Syntax 10R M2.1 V2 Manifest Admission Record

Design source: `docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md`, 10R M2.

Implementation plan: `docs/plans/syntax/10-native-ffi-module-package/m2-manifest-artifact-implementation-plan.md`.

## 状态与产出记录

- 状态：completed
- 开始时间：2026-07-24 20:33 +08:00
- 完成时间：2026-07-24 21:01 +08:00
- 已完成项目：
  - 完成 M2 现状盘点并将 broad manifest/artifact 工作拆成四个可独立验证、独立提交的子里程碑。
  - 新增 v2 base-envelope RED/Green：有效 v2 读取项目 name/version/source/binary/entry 并保留
    `manifestVersion`；六个必填字段逐一缺失、fractional version 与 future version 均被拒绝；explicit 与
    missing `manifestVersion` 的 v1 migration input 都保留为 version 1。
  - 将 v1/v2 version gate 和 v2 base validation 移入独立 manifest module；`project.c` 只保存
    manifestVersion、dispatch validation，并保留 top-level v2 version。
  - GCC、Clang、MSVC 的 isolated focused target 均 3/3，既有 project manifest normalization 均 29/29，
    focused CTest 均 1/1；最终 code review 无剩余阻断项。

## Boundary

M2.1 只建立 `.zrp` v2 base envelope admission，并保留 v1 migration reader。它不解析 v2 aliases、package
exports、dependencies、writer、lock projection、`.zrm` entry 或 provider phase；这些分别由 M2.2-M2.4 完成。
