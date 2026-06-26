# AOT 09-S4C new-owner write barrier elimination

时间：2026-06-25 11:35:46 +08:00

## 范围

- AOT C function-body dispatcher 新增保守线性证明：只有 receiver slot 最近来源为同函数内 `CREATE_OBJECT` / `CREATE_ARRAY`，且中间未跨调用、控制流边界、返回、逃逸或不明 slot 使用时，才视为 new-owner store。
- member、member-slot、index、super-array heap store 在证明成立时改发 `ZrLibrary_AotRuntime_*NewOwnerNoWriteBarrier` 边界。
- core object 层新增 assume-new-owner no-barrier setter，并通过 `skipWriteBarrier` 候选复用现有 object store 流程；实际写入前还会确认 target object 仍为 `YOUNG_MOVABLE`，否则自动回退到 09-S4B `ZrCore_Gc_WriteBarrier`。
- guardrail 将四个新 runtime helper 纳入显式允许边界，防止 no-barrier helper 被误归类为 VM fallback。

## RED / GREEN

- RED：`zr_vm_aot_c_global_contracts_test` 先失败，缺少 no-barrier writer、runtime helper、object API 与 super-array fallback。
- RED：`zr_vm_aot_c_guardrail_contracts_test` 在新增 no-barrier runtime helper 断言后失败，证明 guardrail allowlist 尚未接受这些边界。
- RED：补强源码合同要求 core no-barrier API 出现 young-owner 运行时确认，避免 allocation safepoint 后 owner 已晋升仍盲跳屏障。
- GREEN：source contract 锁定 writer/API/runtime/helper 路由和 young-owner guard；guardrail 明确允许四个 `*NewOwnerNoWriteBarrier` runtime boundary。

## 验证

- WSL gcc direct：global contracts 9/0；guardrail 6/0；global smoke 10/0；root-frame 5/0；constant contracts 5/0；super-array contracts 1/0；super-array smoke 1/0。
- WSL clang direct：global contracts 9/0；guardrail 6/0；global smoke 10/0；root-frame 5/0；constant contracts 5/0；super-array contracts 1/0；super-array smoke 1/0。
- Windows MSVC Debug direct：global contracts 9/0；guardrail 6/0；global smoke 10/0（10 ignored，Unix shared-library path）；root-frame 5/0；constant contracts 5/0；super-array contracts 1/0；super-array smoke 1/0（1 ignored）。

## 备注

- 本切片完成 09-S4；结合已完成的 09-S1、09-S2、09-S3 与 09-S5，09 阶段计划切片已关闭。
- `LOCAL_ADDRESS` 根登记、长时间 GC 压测、card-table 优化与 pinned-region demotion 仍为扩展项，不在本切片完成声明内。
- `object.c`、`aot_runtime.c` 与 `backend_aot_c_function_body.c` 已超过大文件阈值；本切片只接入窄的 flag/boundary/proof plumbing，未混入结构性拆分。
