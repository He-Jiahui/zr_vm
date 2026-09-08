---
related_code:
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/function_precall_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c
  - zr_vm_core/src/zr_vm_core/execution/execution_internal.h
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/call_info.h
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/include/zr_vm_core/stack.h
  - zr_vm_core/include/zr_vm_core/profile.h
  - zr_vm_core/src/zr_vm_core/profile.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/include/zr_vm_core/profile.h
  - zr_vm_core/src/zr_vm_core/profile.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
plan_sources:
  - user: 2026-08-29 optimize VM toward Lua/C# performance
  - docs/plans/benchmark/optimize/00-audit-and-baseline.md
tests:
  - tests/core/test_frame_slot_layout_lookup.c
  - tests/core/test_execution_numeric_fast_paths.c
  - tests/core/test_execution_add_stack_relocation.c
  - tests/core/test_precall_frame_slot_reset.c
  - tests/core/test_postcall_fast_paths.c
  - tests/acceptance/2026-08-29-frame-slot-layout-dense-lookup.md
  - tests/acceptance/2026-08-30-mixed-service-loop-frame-fast-paths.md
doc_type: milestone-detail
---

# 解释器热路径优化计划

> **Goal:** 消除每条 quickened 指令仍承担的帧解析、完整值维护和调用边界成本，把解释器代表集推进到 Lua 2 倍以内。
>
> **Architecture:** 帧创建/加载时验证元数据，执行时使用自校验 O(1) 索引和直接地址；标量 typed lane 保持未装箱表示，只在 GC、反射、闭包、native 和动态语义边界物化完整 `SZrTypeValue`。
>
> **Tech Stack:** C11、computed goto、现有 quickening/profile 框架、Unity、Callgrind、ASan/Valgrind。

## 已完成：稠密 frame-slot 查找

`ZrCore_Function_FindFrameSlotLayout` 已先检查 `frameSlotLayouts[stackSlot]`，只有记录槽号相等时才命中，其他情况保留线性回退。接受证据见 `tests/acceptance/2026-08-29-frame-slot-layout-dense-lookup.md`。

## Task 1：把 frame place 验证移出每次操作数访问

当前实现状态：安全切片和确定性 Callgrind 子门禁已完成，numeric 墙钟子门禁
仍开放。编译器完成 frame layout、loader 完成输入验证后，为完整、非 alias
的 VALUE 槽设置 `ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE`；该派生位不进入
`.zro`，加载端重新计算，运行期 frame 初始化只读元数据。执行 getter 先探测
`frameSlotLayouts[stackSlot]` 的规范记录并直接按当前 frame base 加
`byteOffset`，只有 direct guard 失败时才调用通用 layout lookup 和
`ZrCore_Function_MakeFrameSlotPlace`。`frame_value_slot_direct` / `checked`
采用 append-only helper ID；热 getter 从 `state->global->profileRuntime` 取一次
profile 指针，避免在每个操作数访问上产生 TLS lookup。

GCC 11.4 Release、CPU 2 affinity 的最终 `numeric_loops` Callgrind 记录
`722,029,136 Ir`，相对修改前的 `820,818,823 Ir` 下降 `12.04%`；direct / checked
计数为 `10,006,568 / 1`。`ZrCore_Stack_MakeFramePlace` 独占 `14,352 Ir`
（`0.00199%`），`ZrCore_Function_FindFrameSlotLayout` 独占 `5,424 Ir`
（`0.00075%`），因此 `<5%` 子门禁通过。direct offset、relocation、alias/错位/
越界拒绝、初始化保持、helper ID/计数与 dense/sparse 边界在 GCC、Clang、MSVC
均通过 12/12。

同一 process scope 的 20 样本墙钟结果全部 `UNSTABLE`：C、ZR interp、ZR binary
的 CV 分别为 `17.64%`、`22.48%`、`15.27%`，均为 `gate_eligible=false`。
虽然 ZR interp 中位数从 `755.624 ms` 降至 `635.988 ms`，C 中位数也漂移
`10.17%`，因此不接受 numeric `+10%` 声明。

### Mixed-service frame follow-up（2026-08-30）

`mixed_service_loop` 的 profile 表明通用 frame-place 不只存在于操作数 getter，
还存在于帧初始化、交叠检查、frame pointer 反查、drop，以及上一调用帧 storage
top 的元数据重算。本轮把已验证的 `DIRECT_VALUE` 证明复用到这些边界：公共
`ZrCore_Function_MakeFrameSlotPlace` 先尝试有界 direct place；初始化、交叠和
drop 对 direct VALUE 直接操作；规范逻辑栈指针按 slot 差值 O(1) 反查。任何
alias、inline、稀疏、错位、越界或非规范记录仍进入原 checked path，且所有
地址都从当前 frame base 计算，不跨栈重定位缓存裸指针。

VM precall 同时把本次分配得到的 frame storage slot 数以 24 位 `count + 1`
写入 `SZrCallInfo` 既有 return-flag padding，不改变结构大小或后续字段偏移。
`ZrCore_Function_GetCallInfoFrameStorageTop` 优先读取这个不可变的
per-call 边界；值为零的 legacy/native call info 保留原元数据扫描回退。这个
字段不缓存 `SZrFunction` 派生状态，超过 24 位容量时也回退扫描，因此不会在
quickening、共享函数或并发 mutator 之间形成可变缓存。

相同 GCC 11.4 Release、CPU 2 affinity、scale-1 输入及 checksum `408940136`
下，Callgrind 从 `868,860,510 Ir` 降至 `409,692,473 Ir`，累计下降
`52.85%`。其中 per-call storage count、direct reverse mapping 与 direct drop
分别带来后续阶段 `25.74%`、`13.33%`、`4.03%` 的下降。此结果接受确定性
指令数优化，不接受 `mixed_service_loop +10%` 墙钟门禁；当前 calibrated
process 数据仍不足以形成稳定 paired series。完整证据见
`tests/acceptance/2026-08-30-mixed-service-loop-frame-fast-paths.md`。

**Files:**

- Modify: `zr_vm_core/include/zr_vm_core/function.h`
- Modify: `zr_vm_core/src/zr_vm_core/function.c`
- Modify: `zr_vm_core/src/zr_vm_core/function_frame_place.c`
- Modify: `zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c`
- Modify: `zr_vm_core/src/zr_vm_core/io_runtime.c`
- Modify: `zr_vm_core/include/zr_vm_core/profile.h`
- Modify: `zr_vm_core/src/zr_vm_core/profile.c`
- Test: `tests/core/test_frame_slot_layout_lookup.c`
- Test: `tests/core/test_execution_add_stack_relocation.c`

**Steps:**

1. 添加测试，要求规范 VALUE 槽重复读取不调用 checked place helper，同时稀疏、错位、越界和栈重定位仍走 checked path。
2. 在 profile helper 枚举中增加 `frame_value_slot_direct` 与 `frame_value_slot_checked`，先确认当前核心用例 checked 次数与操作数访问同阶。
3. 在函数加载/构建完成时验证 dense layout 的 slot、offset、size、align 和 frame byte bounds，并缓存只读 canonical 标志；标志保持 append-only，不改变既有字段偏移。
4. 对已验证 VALUE slot 直接用当前 frame base 加预计算 byte offset 得到地址；每次栈重定位后使用新的 base，不缓存裸地址。
5. 非规范/外部构造 function、inline struct slot 和验证失败继续调用 `ZrCore_Function_MakeFrameSlotPlace`。
6. 运行 numeric scale-1 Callgrind；接受条件是 `ZrCore_Stack_MakeFramePlace` 独占比例从 26.16% 降到 5% 以下，core numeric 中位数再提升至少 10%。

## Task 2：typed scalar lane，减少 48 字节值维护

**Files:**

- Modify: `zr_vm_core/include/zr_vm_core/function.h`
- Modify: `zr_vm_core/include/zr_vm_core/stack.h`
- Modify: `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`
- Modify: `zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c`
- Test: `tests/core/test_execution_numeric_fast_paths.c`
- Add: `tests/core/test_typed_scalar_lane_materialization.c`

**Steps:**

1. 为 signed/unsigned/f64/bool 本地槽增加 typed-lane 元数据测试，先覆盖算术、分支、循环、异常、闭包捕获、native 参数和 debug 读取。
2. 用 frame byte sidecar 保存 8 字节 payload 和有效/dirty 状态，不改公开 `SZrTypeValue` ABI。
3. quickened typed 指令直接读写 payload；普通动态指令进入前物化完整值，写回后按类型重新专化或失效。
4. GC 只扫描已物化且可 GC 的 VALUE 槽；纯标量 lane 不进入对象/所有权 barrier。
5. debug、reflection、closure capture、yield、exception unwind 和 native boundary 必须调用同一 materialization helper。
6. 接受条件：numeric stress 至少再提升 30%，所有 value/ownership/GC/stack relocation 测试通过，Callgrind 中每条整数指令不再出现完整值清理 helper。

## Task 3：基于真实 trace 选择 superinstruction

**Files:**

- Modify: `zr_vm_core/include/zr_vm_core/profile.h`
- Modify: `zr_vm_core/src/zr_vm_core/profile.c`
- Modify: `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c`
- Modify: `zr_vm_common/include/zr_vm_common/zr_instruction_conf.h`
- Test: `tests/core/test_execution_numeric_fast_paths.c`
- Test: `tests/instructions/test_instructions.c`

**Steps:**

1. 增加相邻 opcode pair/triple 的有界 profile 计数，不在普通 Release 路径分配内存。
2. 在 numeric、branch、dispatch、array 和 mixed-service profile 档收集 trace。
3. 只实现累计覆盖至少 5% retired bytecodes、且不跨异常/安全点/GC 边界的组合。
4. quickening 时验证槽类型、常量和跳转目标；任何 guard 失败回退原指令序列。
5. 每个新 opcode 增加 encode/decode、binary roundtrip、debug single-step 和 profile name 测试。
6. 单个 superinstruction 若目标 case 提升小于 3% 或代表集回退超过 1%，删除该指令。

## Task 4：调用与返回边界

当前实现状态：第 2 步中的 frame storage count、VALUE 参数布局摘要与 direct VALUE-only
frame-drop 摘要及其 frame-initialization 复用已完成独立安全
切片。storage count 属于具体 `SZrCallInfo`，只描述该次 precall 或 tail reuse 已分配的
storage 边界；参数摘要属于已发布 `SZrFunction` 的不可变派生元数据，只描述规范 direct
VALUE 参数数量和所需 layout 前缀。callee identity、单返回目的地专化和
generation/token 失效仍未完成，因此 Task 4 整体及 `dispatch_loops` /
`call_chain_polymorphic` 门禁保持开放。

### VALUE 参数布局摘要（2026-08-30）

finalize 仅在完整 layout 规范且所有参数都有 loader/compiler 重建的 `DIRECT_VALUE` 证明
时写入 `directValueParameterCountPlusOne` 与 `directValueParameterScanLength`。摘要先清零再
构建，不进入 `.zro`；运行期若 bounds、计数或最后一个 direct 参数校验失败，保留原 full
scan 与 checked place。零参数调用不访问 layout，direct copy 仍同步 byte-backed 与 dense
两份视图，并保留覆盖前 ownership release。

精确配对的 GCC 11.4 Release、CPU 2、scale-1 `mixed_service_loop` Callgrind 从
`409,431,558 Ir` 降至 `396,430,578 Ir`（`-3.18%`），checksum 仍为 `408940136`。
目标函数 exclusive/inclusive 分别下降 `38.27%` / `37.91%`；helper profile 为 direct
`61,449`、layout visit `61,449`、empty `2`、checked `0`。`numeric_loops` 为 `-0.012%`，
`object_field_hot` 为 `+0.005%`，通过代表集 `1%` 退化门禁。完整证据见
`tests/acceptance/2026-08-30-value-parameter-layout-summary.md`。

### Direct VALUE 参数同址 copy 专化（2026-08-31）

strict 参数摘要命中时，普通 call-window copy 先对完整 `frameByteSize` 做一次边界预检，
然后直接使用 finalize 已验证的参数 offset。当 `argumentBase == calleeFrameBase` 时，参数源
就是目标 dense mirror；只需写 byte-backed mirror，不再释放并从 byte copy 回完全未变化的
dense 源。独立 argument window 仍同步两份 mirror，`ZrCore_Value_Copy` 仍负责 stale control
归一化和 owner release；frame-source 与 checked 路径不变。

最终精确配对为 `296,648,172 -> 282,552,302 Ir`（`-4.752%`，checksum
`408940136`）。参数 copy inclusive 为 `21,688,474 -> 7,372,972 Ir`（`-66.01%`）；
`numeric_loops` / `object_field_hot` 仅 `+0.0012%/+0.0001%`。完整证据见
`tests/acceptance/2026-08-31-direct-value-parameter-in-place-copy.md`。

### Direct VALUE-only frame-drop 摘要（2026-08-31）

finalize 仅在全部 frame layout 都是规范、边界已验证且无额外生命周期 flag 的 direct
VALUE 槽时写入 `directValueFrameSlotCountPlusOne`；零值保留原 registry、inline preflight、
inline drop 与 checked VALUE place 路径。摘要不序列化，由 loader/compiler finalize 重建。
direct 循环最初仍逐槽调用 bounded place helper，并分别处理 byte-backed/dense owner 与 overlap。
后续切片把等价边界证明提升为一次完整 `frameByteSize` 预检；finalize 已证明每个 direct
layout 都位于该 span 内。两个 mirror 都是 `NONE` ownership 时跳过不可能释放 owner 的 helper
与 overlap probe，任一 mirror 带其他 ownership kind 时仍执行原逻辑。checked/mixed/inline 路径
完全不变。

精确配对 GCC 11.4 Release、CPU 2、scale-1 `mixed_service_loop` Callgrind 为
`396,142,221 -> 378,649,763 Ir`（`-4.42%`），checksum `408940136` 不变；目标函数
exclusive/inclusive 分别下降 `47.27%` / `35.59%`。helper profile 为 direct `20,485`、
checked `1`。`numeric_loops` 为 `-0.021%`，`object_field_hot` 为 `-0.0009%`，通过代表集
`1%` 退化门禁。完整证据见
`tests/acceptance/2026-08-31-direct-value-frame-drop-summary.md`。

完整 span 预检单独把最新基线 `314,490,481` 降至 `306,794,357 Ir`（`-2.447%`），
未达到独立 `3%` 门槛；加入精确 no-owner pair skip 后降至 `296,648,172 Ir`
（组合 `-5.673%`）。`numeric_loops` / `object_field_hot` 分别仅 `+0.0043%` /
`+0.0058%`。drop 函数 exclusive 从 `19,658,273` 降到 `9,530,544 Ir`，owner helper
从 `7,435,923` 降到 `1,353 Ir`。完整证据见
`tests/acceptance/2026-08-31-direct-frame-drop-span-and-no-owner-skip.md`。

### Direct VALUE-only frame initialization（2026-08-31）

`ZrCore_Function_InitializeFrameLayoutStorage` 复用严格的
`directValueFrameSlotCountPlusOne` 证明，不新增或放宽信任状态。direct 路径按 layout
顺序线性计算参数序号，保留前 `preservedArgumentCount` 个参数，其余槽仍通过 bounded
direct helper 计算当前 frame base 内的地址。无摘要、mixed/inline、alias、手工或未 finalize
布局继续执行原 checked 初始化逻辑。

精确配对 GCC 11.4 Release、CPU 2、scale-1 `mixed_service_loop` Callgrind 为
`378,637,009 -> 365,295,917 Ir`（`-3.52%`），checksum `408940136` 不变；目标函数
exclusive/inclusive 分别下降 `62.83%` / `61.51%`。helper profile 为 direct `20,486`、
checked `1`。`numeric_loops` 为 `-0.055%`，`object_field_hot` 为 `-0.005%`，通过代表集
`1%` 退化门禁。完整证据见
`tests/acceptance/2026-08-31-direct-frame-initialization.md`。

### Dispatch direct VALUE getter inline（2026-08-31）

`FRAME_VALUE_SLOT` 现在在 `execution_dispatch.c` 内先调用 direct probe；probe 只处理
规范、索引一致且带 `DIRECT_VALUE` 证明的 VALUE layout，并使用循环已缓存的
`profileRuntime` / `recordHelpers`。任何 guard miss 返回空值后调用原通用 getter，checked
layout lookup、place 构造、边界与 inline layout 语义保持不变。所有地址仍由本次操作数读取时的
frame base 派生，不跨栈重定位保存裸指针。

最终外层 wrapper 使用普通 `inline`，允许 GCC 对低频调用点保留 outlined 函数。强制内联候选为
`348,704,899 Ir`，普通内联候选为 `349,179,948 Ir`，差异仅 `0.14%`；后者核心共享库比
强制版本小 `24,504` 字节，因此保留普通内联。相对上一接受二进制，精确配对结果为：
`mixed_service_loop` `365,326,562 -> 349,179,948 Ir`（`-4.42%`）、
`numeric_loops` `134,635,195 -> 111,726,116 Ir`（`-17.02%`）、
`object_field_hot` `126,304,766 -> 110,883,674 Ir`（`-12.21%`），checksum 均不变。
profile direct/checked 为 `1,890,775 / 30,725`。核心共享库相对 before 增加
`90,112` 字节（`3.51%`），GCC Release 增量目标构建为 `449.59s`、峰值
`1,280,792 KiB`；性能收益与构建/体积成本同时接受。完整证据见
`tests/acceptance/2026-08-31-dispatch-frame-value-slot-inline.md`。

### Direct VALUE-to-VALUE copy probe skip（2026-08-31）

`execution_frame_value_slot_copy_requires_inline_probe` 现在让源和目标都带规范
`DIRECT_VALUE` 证明的普通 VALUE copy 在空值/返回槽检查后直接跳过 inline-copy
探测。这类 pair 不可能是 inline payload 或 alias；调用者继续执行原
`SZrTypeValue` copy。任一侧不能证明时，原 inline struct、union、constructor
carrier、ownership 与 checked getter 路径完全保留。

精确配对的 `mixed_service_loop` 为
`349,179,948 -> 325,175,994 Ir`（`-6.87%`，checksum `408940136`）；
`numeric_loops` 为 `111,726,116 -> 111,767,607 Ir`（`+0.037%`），
`object_field_hot` 为 `110,883,674 -> 110,881,829 Ir`（`-0.002%`），均通过
`1%` 代表集退化门禁。profile direct/checked 由 `1,890,775 / 30,725` 变为
`1,582,901 / 30,725`，恰好减少 `307,874` 次 direct speculative getter。
`execution_inline_frame_try_copy_stack_slot` inclusive 从 `35,097,636` 降到
`11,083,464 Ir`（`-68.42%`）。独立小头文件避免此 follow-up 触发大型
dispatch TU 重编译；最终 GCC 增量只编译 `execution_inline_frame.c`，核心库和
dispatch object 体积不变。完整证据见
`tests/acceptance/2026-08-31-direct-value-copy-probe-skip.md`。

### Dispatch strict-summary copy-probe bypass（2026-08-31）

普通与 fast `GET_STACK` / `SET_STACK` 调用点现在先检查不可变的 strict direct VALUE-only
frame 摘要和两个 slot 的 layout bounds。命中时直接证明 inline probe 不可能需要，避免进入
`execution_inline_frame_try_copy_stack_slot`；没有摘要时仍执行原逐槽 direct predicate，确有
inline 可能时才调用原 helper。helper 内部继续重复 fail-closed predicate，所有 mixed、inline、
union、constructor carrier、未 finalize、越界路径保持原语义。新增 append-only
`frame_value_copy_probe` helper profile 项，使实际 fallback 可观测。

逐槽 predicate 候选只得到 `282,552,302 -> 274,182,784 Ir`（`-2.961%`），低于独立
`3%` 门槛而拒绝。strict-summary 最终精确配对为 `282,552,302 -> 273,765,184 Ir`
（`-3.110%`，checksum `408940136`）；numeric/object 分别为 `-0.0060%/-0.0084%`，
最终 mixed profile 中 copy-probe helper 为零。相对初始 mixed baseline 累计下降
`68.491%`。完整证据见
`tests/acceptance/2026-08-31-dispatch-direct-value-copy-probe-bypass.md`。

### Packed direct VALUE frame boundary（2026-08-31）

`directValueFrameSlotCountPlusOne` 现在只为更严格的 packed frame 发布：layout 数必须等于
`stackSize`，每个 direct VALUE byte mirror 必须紧跟 dense frame 并按
`sizeof(SZrTypeValueOnStack)` 步进，payload 大小必须精确等于 `sizeof(SZrTypeValue)`，且
parameter 必须构成 `parameterCount` 前缀。非 packed 规范槽仍保留逐槽 `DIRECT_VALUE` 标记，
但 strict frame summary 为零并进入原 checked/per-layout 路径。

packed 证明让 precall 直接排除 inline 参数扫描，并让参数 copy、初始化和 drop 各自只派生一次
byte mirror base 后按固定 stride 遍历。返回值、receiver 与 constructor receiver 的 inline-copy
探针也会对 strict direct-only callee 立即返回；caller inline object destination 的独立 fallback
不变。prepared-precall fast guard 没有放宽，因为目标 frame 的 byte storage 超过逻辑 stack，且
GC-safe entry clear 必须覆盖完整逻辑 frame。

precall scan-only 候选仅 `-0.655611%`；加入 packed loops 后仅 `-2.839406%`，均未独立通过
`3%` 门禁。最终组合精确配对为 `273,765,184 -> 255,021,394 Ir`（`-6.846667%`，
checksum `408940136`）；numeric 为 `-0.012560%`，object 为 `+0.010445%`，均通过 `1%`
代表集门禁。相对初始 mixed baseline 累计下降 `70.648753%`。四配置 13 项矩阵通过，frame
更新为 `35/35`。完整证据见
`tests/acceptance/2026-08-31-packed-direct-value-frame-summary.md`。

### Packed direct VALUE drop owner batching（2026-08-31）

strict packed drop 不再把内部 dense mirror 读取记为通用 stack getter。direct 循环用
`ZrCore_Stack_GetValueNoProfile`，并把四个 byte mirror 与四个 dense mirror 的 ownership kind
合并判断；八个值全为 `NONE` 时整组跳过，任一值可能持有 owner 时仍逐 pair 执行原
releasable-owner 检查并独立释放两份 mirror。尾部不足四槽的部分使用同一 pair 逻辑。无 strict
packed summary 的 frame 保留原 registry/inline/alias/unwind/checked/overlap 路径。

no-profile getter 单独为 `255,021,394 -> 248,600,396 Ir`（`-2.517827%`），未通过独立
`3%` 门禁。加入四槽预判后的最终精确配对为 `255,021,394 -> 245,339,382 Ir`
（`-3.796549%`，checksum `408940136`）；numeric 为 `+0.020328%`，object 为
`-0.015146%`。drop exclusive 下降 `61.240%`，TLS helper 下降 `63.647%`；四配置 13 项
矩阵通过，frame 更新为 `36/36`。完整证据见
`tests/acceptance/2026-08-31-packed-direct-value-drop-owner-batching.md`。

### Packed direct prepared precall fusion（2026-08-31）

prepared VM call 现在只在 debug hook 关闭、参数数量精确、call window 精确、strict packed
summary 命中、现有栈容量覆盖完整 byte frame、entry clear 有界且下一 call-info 已可复用时进入
专用路径。任一 guard 失败仍进入原 exact probe/generic precall，因此首次 call-info 分配、栈扩容、
debug、inline/alias/mixed layout 与非精确参数路径不变。

专用路径仍清空完整逻辑 entry frame 与所有 storage padding。packed proof 说明固定 stride byte
mirror 全部位于该 padding 区域，因此无需随后重复 layout initialization；参数直接从 dense prefix
通过 `ZrCore_Value_Copy` 写入 byte mirror，ownership 与 helper profile 语义保持不变。generic copy
加 layout init 的首版仅 `-1.611%`，复用 padding clear 后仍仅 `-2.609%`，均未独立接受。最终精确
copy/private helper 结果为 `245,339,382 -> 236,125,782 Ir`（`-3.755451%`），numeric/object 为
`-0.003641%/+0.013457%`，累计 mixed 降幅为 `72.823511%`。四配置 13 项矩阵通过，precall
更新为 `18/18`。完整证据见
`tests/acceptance/2026-08-31-packed-direct-prepared-precall-fusion.md`。

### Packed signed scalar frame-base cache（2026-09-01）

这是 Task 2 的第一段地址侧切片，不改变 `SZrTypeValue`、公开 stack ABI 或所有权语义。
strict packed direct VALUE frame 在 outer dispatch 解析当前函数后只计算一次固定 stride byte
mirror base；signed 算术、比较、转换、fused load、branch operand 与完整 VALUE destination
直接使用该 base。call-info 的 function base 与已解析 frame identity 不一致时立即清空 cache，
outer loop 重新解析后才恢复；summary 缺失、越界、relocation/call switch 均回退原 direct/checked
getter。unsigned/float/string/object/ownership/inline layout handler 不变。

逐访问重复 summary 检查的首版虽通过测试，但 numeric 从 `111,746,343` 退化到
`119,654,940 Ir`（`+7.076%`），已拒绝。最终 once-per-frame 版本为：numeric
`111,746,343 -> 94,984,122 Ir`（`-15.000241%`）、mixed
`236,125,782 -> 237,158,413 Ir`（`+0.437322%`）、object
`110,875,378 -> 103,881,355 Ir`（`-6.308004%`），checksum 均不变。profile 的
direct/checked 仍为 `2,502,333 / 1`；四配置 13 项矩阵通过，frame 更新为 `37/37`。
完整 typed payload、dirty/materialization 与 Task 2 的 `30%` 总门禁仍开放。完整证据见
`tests/acceptance/2026-09-01-packed-signed-scalar-frame-base.md`。

### Immutable generated frame-slot count summary（2026-08-31）

`generatedFrameSlotCountPlusOne` 现在把完整指令流扫描结果发布为不可变派生摘要；
零值仍调用原扫描，因此未 finalize 或手工构造函数保持动态语义。摘要不进入
`.zro`，loader 在复制完整指令流后重建，compiler 的零栈与普通布局路径都会发布，
quickening 在全部重写完成后再次发布。重新 finalize 会先清零并刷新摘要；无法用
`count + 1` 表示时继续扫描。

同一 GCC 11.4 Release、CPU 2、scale-1 精确配对把 `mixed_service_loop` 从
`325,175,994` 降到 `314,490,481 Ir`（`-3.286%`，checksum `408940136`）。
`numeric_loops` 为 `-0.023%`，`object_field_hot` 为 `-0.012%`，通过 `1%`
代表集门禁。公共 getter 从 `10,624,761` 降到 `163,904 Ir`，初始化期扫描仅
`17,347 Ir`。完整证据见
`tests/acceptance/2026-08-31-generated-frame-slot-count-summary.md`。

**Files:**

- Modify: `zr_vm_core/src/zr_vm_core/function.c`
- Modify: `zr_vm_core/include/zr_vm_core/function.h`
- Modify: `zr_vm_core/src/zr_vm_core/function_frame_place.c`
- Modify: `zr_vm_core/src/zr_vm_core/function_precall_internal.h`
- Modify: `zr_vm_core/src/zr_vm_core/execution/execution_internal.h`
- Modify: `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`
- Add: `zr_vm_core/src/zr_vm_core/execution/execution_frame_value_slot_fast.h`
- Modify: `zr_vm_core/include/zr_vm_core/profile.h`
- Modify: `zr_vm_core/src/zr_vm_core/profile.c`
- Test: `tests/core/test_frame_slot_layout_lookup.c`
- Test: `tests/core/test_postcall_fast_paths.c`
- Test: `tests/core/test_tail_reuse_callinfo_reset.c`
- Test: `tests/core/test_vm_closure_precall.c`

**Steps:**

1. 用 helper profile 分开统计 monomorphic known VM call、polymorphic cache hit/miss、frame allocation、argument copy 和 return copy。
2. 为单态 VM 调用缓存 callee、frame storage count、参数布局摘要和单返回目的地；generation/token 不匹配时失效。
3. 对纯标量参数和返回值使用 typed-lane copy；owner、borrowed、weak 和 inline struct 保留现有生命周期路径。
4. 尾调用复用必须继续清除 call info、pending control flow 和 owned slot；不得以性能为由放松现有测试。
5. 接受条件：`dispatch_loops` 与 `call_chain_polymorphic` 各提升至少 15%，cache miss/语义回退不回归。

## Task 5：安全点和热代码体积

**Files:**

- Modify: `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`
- Add: `zr_vm_core/src/zr_vm_core/execution/execution_dispatch_numeric.inc`
- Add: `zr_vm_core/src/zr_vm_core/execution/execution_dispatch_call.inc`
- Add: `zr_vm_core/src/zr_vm_core/execution/execution_dispatch_object.inc`
- Test: `tests/core/test_gc_concurrent_major.c`
- Test: `tests/core/test_execution_numeric_fast_paths.c`

**Steps:**

1. 先用硬件计数器可用环境测 branch miss、I-cache miss 和 IPC；没有这些证据不得仅凭 9,500 行文件大小宣称 I-cache 缺陷。
2. 将 opcode handler 宏按 numeric/call/object/control 家族拆到 `.inc`，保持一个 computed-goto 函数和现有 label 地址，不引入函数调用。
3. 把 safepoint poll 从每条指令自增改为按已知 basic-block 长度扣减，只在 backedge、call 和预算耗尽处检查。
4. 并发 GC 最大停顿和取消响应延迟不得超过现有 256 指令预算合同的 1.25 倍。
5. 接受条件：代表集几何平均提升至少 3%，否则只保留可维护性拆分，不声称性能收益。
