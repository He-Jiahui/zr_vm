# Plan 03 Task 7.12 Read-Only Signature Facts Acceptance

## 验收基线

- 基线 HEAD：`c4f9fc504c51983f548826181d7d764d831a3fd9`。
- RED：隐藏 argument expression fact 后 signature request 把 fact 数量从
  `5` 增至 `6`；source contract 同时报出 materializer 与 inference 调用。
- GREEN：同一请求返回 signature help，但 fact 数量保持 `5 -> 5`。

## 验收结果

- GCC/Clang/MSVC focused semantic facts：`13/13`。
- GCC/Clang/MSVC source contracts：`63 Pass / 0 Fail`。
- Full interface：三套均为固定 `109 Pass / 4 Fail`，marker delta 0，不计
  GREEN。

## 状态与产出记录

- 完成时间：2026-08-29 01:11 +08:00。
- 状态：通过本子里程碑验收。
- 完成项目：immutable signature snapshot guard、no-request-inference source
  contract、三工具链真实进程退出与既有 marker delta 复核。
