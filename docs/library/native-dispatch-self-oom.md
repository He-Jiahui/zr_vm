---
related_code:
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - tests/library/test_native_dispatch_self_oom.c
  - tests/cmake/native-dispatch-self-oom-tests.cmake
implementation_files:
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
plan_sources:
  - "user: 2026-10-16 支撑层现役 BUG 修复与真实回归"
tests:
  - tests/library/test_native_dispatch_self_oom.c
  - tests/cmake/native-dispatch-self-oom-tests.cmake
doc_type: failure-contract
---

# Native dispatcher 大参数 self 所有权回归

## 当前诊断与范围

本组已取得实际 RED，随后补齐两个失败分支的 self 引用释放；修复后 GREEN 待 root 统一验证。普通实例方法在参数数目超过内联容量、没有 STACK_ROOT_CONTEXT 标志时进入 generic lane。该路径先复制 self，再申请稳定参数数组和 pin 标记数组。原先两个 raw 分配失败分支直接返回，遗漏已复制 self 的强引用释放；第二分支虽然释放参数数组，仍未释放 self。现在两处分支在返回前按 hasCopiedSelf 释放 stableSelfCopy，第二分支保留参数数组释放。本组不修改独立的 raw stackTop 搬迁候选，也不规定此前未约定的 raw OOM 返回布尔值或错误状态。

## 真实入口与故障定位

夹具使用实际 GlobalState、NativeRegistry Attach/RegisterModule、Module_ImportByPath、FindPrototype/NewInstanceWithPrototype、Object_GetValue 和公开 ZrLib_CallValue。静态 CLASS 描述符提供非 static 方法，参数数目为当前内联容量加一，dispatchFlags 为零；参数全部为整数，避免参数语义克隆引入额外对象分配。receiver 通过公开 SharePlainValue 建立唯一 caller shared owner，再建立 weak 观察者和独立 GC lease。

准备阶段完成后，分配器只在 old=NULL、oldSize=0、OBJECT 类别和精确字节大小匹配时触发。稳定参数数组失败用例放过 CallValue 的第一个同尺寸数组，拒绝 dispatcher 的第二个；pin 标记失败用例放过两个值数组，拒绝第一个精确 bool 数组。测试断言两种尺寸的完整命中次数、唯一拒绝、回调未进入，并记录拒绝时 caller、outer receiver 栈槽和 stableSelfCopy 三份强引用。正常对照应进入一次回调并返回整数 73。

## 区分栈槽所有者与遗失 C 引用

CallValue 会在 scratch receiver 栈槽复制一次所有权；正常 generic 调用还会建立额外的稳定 self 栈槽。调用后直接比较强引用数会把这些可定位栈槽与待测 C 局部引用混淆。

夹具在目标分配拒绝时，从真实活动 callInfo 的 functionBase+1 记录 receiver 栈槽锚点；正常回调另外记录实际 argumentBase 前一槽的稳定 self 锚点。调用完成后，只在这些实际槽仍为同 control 的 shared owner 时，通过公开 ReleaseValue 释放该槽。若 Core 已经消费槽，则跳过；不扫描任意内存，不构造 stableSelfCopy 的替身，也不按剩余计数循环扣引用。

清理这些具体栈槽后，强引用应只剩 caller 的初始一份；旧实现诊断预测多留一份 C 局部引用。随后释放 caller，强引用应归零，weak Wake 应成功得到 null。RED 时 Wake 可能新建一份合法 shared owner，夹具只释放这份真实返回 owner，绝不补偿原先遗失引用。

最后释放 weak、lease 并销毁 GlobalState，验证全类别 VM 分配的大小/类别匹配和零存活块。强制销毁可能使遗失控制块失效，因此销毁后零分配不能替代销毁前的强引用和 weak 断言。显式 GC lease 还保证最后一份 owner 释放后查询 raw 对象安全。

## 注册与验证边界

目标为 `zr_vm_native_dispatch_self_oom_test`，CTest 前缀 `native_dispatch_self_oom_`，三个独立进程：stable_arguments_oom、pin_flags_oom、oversized_control。每项 30 秒超时，Windows 禁止崩溃对话框。片段检查全部七个实际链接目标后才注册；不修改顶层 CMake。

Root 的实际 caller-exception-fs-red configure/build 均为 0，1935 inputs 的 changed_inputs=[]。两个故障进程均在拒绝时观测 strong=3，释放一个真实 scratch 槽后仍为 2（baseline=1），释放 caller 后 weak 仍可唤醒，因而明确失败；正常 oversized_control 通过。生产随后只补上述两处 release 与更新已修 BUG 注释，没有改变 raw 分配失败的返回状态、其他 lane 或独立 stack anchor 候选。精确反向撤掉本组 hunk 后完整匹配编辑前文件，保留所有外来改动与 CRLF。agent 未运行 native，修复后的统一实际 GREEN 待 root 执行。测试不覆盖其他 dispatch lane、参数数组尺寸乘法溢出、并发分派或 callback 异常清理。

## Root acceptance

Actual ClangCL configure/build0, 168/168 CTest, pool14, exceptions15 and CLI hello0; 1944 input hashes unchanged; independent review ACCEPT. Windows only. Evidence: `reports/review-bugfix-20261016/integrated-support-batch-6-green-{0..5}.log` and receipt under the configured external artifact root.
