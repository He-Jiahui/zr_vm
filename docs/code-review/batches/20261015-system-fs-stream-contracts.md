---
related_code:
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_registry.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_sweep.c
  - zr_vm_core/src/zr_vm_core/gc/gc_object.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
  - zr_vm_core/src/zr_vm_core/string/string.c
  - zr_vm_core/include/zr_vm_core/string.h
  - zr_vm_core/include/zr_vm_core/hash_set.h
  - zr_vm_core/src/zr_vm_core/base/hash_set.c
  - zr_vm_core/src/zr_vm_core/base/memory.c
  - tests/system/test_system_fs_module.c
  - tests/cmake/compiler-project-targets.cmake
  - tests/cmake/debug-metadata-aot-registration.cmake
implementation_files:
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/system/test_system_fs_module.c
  - tests/cmake/compiler-project-targets.cmake
  - tests/cmake/debug-metadata-aot-registration.cmake
doc_type: workflow-detail
---

# System.Fs FileStream 实现契约审查

## 范围与清点

本批审查的当前 fs_stream.c 为 SHA-256 1CE5D972F70E8B584C5693CD86C010DCED18CE22025AC0A2F4A61FED18ADF754、1000 行；Root 的 native r2 实际运行使用前一注释版 SHA-256 E884524010D451EEB81431FEBA74EEEFD5D38707B92829CEDB8E37589610DA62，当前版本补充共享 File API 结果路径与初始快照查询 TODO 等契约注释，不能把 r2 测试结果写成在 1CE5D972 版本运行。当前版 strict r5 注释等价证明 SHA-256 DDA2B30EF831D61F426FBAB245CA83BDA7012B916820AB7B5696A7F77B2F5C14 已通过 3467 tokens/2 lexer；后续 tensor-stream-final-native-r1 已在该当前版本构建并运行现有 suites，结果和限制见本批末段。实施文件清点为：21 个函数（3 个文件内 static helper、6 个 owner/生命周期 helper、4 个读写 helper、8 个注册 callback）、1 个 @file 职责单元及 9 个独立失败或资源时序块，共 31 个唯一 (file, unit, kind) 项。coverage 当前分类为 15 个 commented、6 个 BUG、10 个 TODO，无 no-comment 项。BUG 标签有 6 条台账行，对应 3 类窄路径：owner setter 软失败、文本首 NUL 截断、短串缓存扩容失败。测试源码只作只读证据，本批不重复登记或修改共享 test unit。

## 注册、owner 与构造时序

File.Open 在 fs_entry.c:389-399 打开宿主句柄，并于 :394 同步调用 NewStreamObject；仅正常返回 NULL 时进入 :395-396 的 CloseHandle 清理，非 NULL wrapper 于 :399 被交付。NewStreamObject 的 TempValueRoot 在 fs_stream.c:140 建立，但对象要等 Type_NewInstance 与 record malloc 都成功后才于 :157 加入根；随后 :159-165 安装 finalizer、尝试写 owner 与字段。malloc 创建的 ZrSystemFsStreamData 由 __zr_fs_stream 隐藏字段持有；__zr_ffi_handleId 是 FFI lowering 的整数视图，不拥有宿主句柄。

owner setter 是 void。受限 owner BUG 需要 ignored-root count=capacity-1：构造入口对 receiver 的 pin 占据最后一个槽，紧接着字段名字符串的 ARRAY 扩容 RawMallocWithType 失败；其余对象、记录和字段步骤仍成功。native_binding_dispatch.c:1385-1404 的 setter pin 失败早退且不写字段，NewStreamObject 不检查该 void setter，仍返回 wrapper。File.Open 因返回非 NULL 而不关闭句柄；之后 GetStreamData 在 owner 缺失时返回 NULL，finalizer 也找不到 record，导致 record 和句柄失去本模块的释放路径。allocator 条件、唯一失败点和其余步骤成功均为静态路径证明，没有故障注入；不泛化为任意 OOM。

path 参数只在 NewStreamObject 的同步调用期借用。File_Open 在到达该调用前还会解析路径并经历分配；fast/inline/generic lane 的参数 pin、GC 时点与指针有效性仍列 TODO，没有证据证明指针悬空。另有 Type_NewInstance 非局部 MemoryError 绕过 File_Open 的 NULL-only 清理 TODO，也未升级为 BUG。

## Finalizer 派发与关闭边界

NewStreamObject 于 fs_stream.c:159 把 ZrSystem_Fs_StreamFinalize 赋给 object->super.scanMarkGcFunction；这是函数指针注册，不是 caller。已闭合两条普通 OBJECT 释放入口：完整周期 gc_cycle.c:2890-2893 进入 garbage_collector_free_object，再经 gc_object.c:1053-1066 和 :1008-1013 分派；增量 sweep gc_sweep.c:27-35 进入 sized free，再经 gc_object.c:1042-1050 和 :1008-1013 分派。gc_sweep.c:59-73 的 wait-to-release 代码没有已确认的入队点，故该派发路线单列 TODO，不能作为已证 caller。

finalizer 独立尝试 CloseHandle；不论宿主关闭结果如何，它都会把 private handle 置 invalid、closed 置 true、释放 record 并清除 owner。显式 Stream_Close 则经 fs_registry.c:85-86 / :233-234 注册，调用 StreamCloseInternal；close 失败时 helper 保留 private handle 和 closed=false，但底层 OS 句柄是否仍开放、能否安全重试尚未核实。GC finalizer 不调用 StreamCloseInternal，且没有返回槽报告关闭错误。

## I/O 结果与软失败

registry 在 fs_registry.c:193-234 把相同 reader/writer callbacks 同时绑定到 IStreamReader/IStreamWriter 与 FileStream。普通 dispatcher 先按 descriptor 检查显式参数个数，再经 native_binding_dispatch_lanes.h:277 派发 callback。StreamEnsureOpen 只验证 owner、closed 与 invalid handle；readable/writable 由具体 callback 检查。

ReadBytesFromHandle 以 malloc buffer 读取 EOF 或 byte count，再构造 VM array。正常和普通失败路径释放本地 buffer；VM array 由 GC 按可达性管理，未发布 array 没有显式 free。Array_New/PushValue 若通过非局部 MemoryError 越过 buffer free，仍需沿异常恢复链核验。WriteBytesToHandle 会先检查所有 array 元素为 0..255，再复制并循环写出；宿主部分写失败不回滚已交付前缀。WriteTextToHandle 按 strlen 计算，嵌入 NUL 只写前缀，不验证 UTF-8。读写、seek、resize 都先完成底层 I/O，再查询并同步快照；查询失败仍报 IOException，已发生的游标/数据副作用不回滚。void 字段 setter 只代表尝试写字段，不保证公开快照全部更新。

ReadTextFromHandle 读满实际 byte count 后追加 NUL，再用 C-string SetString。合法文件内容 A-NUL-B 会消费完整字节数但只发布首 NUL 前缀；现有 File 文本测试不含 NUL。短串 HASH_BUCKET 软失败是另一条独立窄路径：短串 cache miss 位于 bucket load threshold，且唯一 HASH_BUCKET resize 分配返回 NULL 时，CreateTryHitCache 返回 NULL，void SetString 不改 dispatcher 预置的 null 槽。对 Stream_ReadText，只有其后的 StreamSyncFields 查询成功，callback 才返回 true 且脚本观察到 null；若查询失败则走 IOException/false。该路径已由静态调用链闭合，未做 allocator fault injection。ReadText count 按字节，可切断合法 UTF-8 多字节序列；公开边界策略仍待用例明确。

## 测试证据与到达范围

测试目标由 tests/cmake/compiler-project-targets.cmake:186-199 声明，并由 tests/cmake/debug-metadata-aot-registration.cmake:862-874 注册。test_system_fs_module.c:625-690 的 metadata 用例检查 FileStream descriptor、implements、owner/FFI 字段与 release hook；它不是 finalizer 或 stream callback 行为测试。

File 一次性 I/O 用例 test_system_fs_source_runtime_supports_path_objects_and_directory_operations 在 tests/system/test_system_fs_module.c:733-854 通过；:751 的 File.writeText、:752 的 File.readText、:753-756 的 writeBytes/appendBytes/readBytes 走 fs_entry.c 入口，并实际覆盖本文件共享的 WriteTextToHandle、ReadTextFromHandle、WriteBytesToHandle、ReadBytesFromHandle 普通路径。它不给 Stream_* callback 行为信用；该 fixture 不测 NUL、UTF-8 切分、缓存失败或宿主 I/O 错误。

stream_modes 在 tests/system/test_system_fs_module.c:944-1043。Root 管理的 frozen source native r2 静态 Debug build exit 0；Fs 实测为 7/8，唯一失败是 C1035 KNOWN_NATIVE_MEMBER_CALL_RECV_U8 invalid cached native member callable，在 takeReader 调用附近停止。失败前已执行 :970/:974 两个 stream.writeText 调用及 :973/:975 两次 seek/tellFd 检查。:952 的 takeReader 函数体内有 readText(1)，调用路径在 :976 失败，因此不给 ReadText 成功 callback 行为信用；:978 的 readText(-1)、:979 setLength、:981-982 close 及 :985 之后的 writer、:990-992 bytes 操作均未执行。模板没有 flush 调用。closed-stream 用例 :1126 确实到达 close，但只在 :1153 断言后续执行失败，不检查 close 自身返回、异常种类或 OS 句柄状态。:1216-1339 的 tellFd/native probe 只编译模板，不执行 callbacks。上述实际运行由 Root 管理，本 producer 未运行测试，也未添加测试。

## BUG、TODO 与证据映射

- owner 字段软失败：NewStreamObject / owner 发布窗口两条 BUG 行对应同一受限单故障链，见 fs_stream.c:159-167、fs_entry.c:394-399、native_binding_dispatch.c:1385-1404、gc.c:696-705、gc_object.c:722-756、fs_stream.c:251-254。没有 fault injection。
- 首 NUL 截断：ReadTextFromHandle / 首 NUL block 两条 BUG 行描述同一 helper 语义，见 fs_stream.c:342-348、native_binding_dispatch.c:1303-1308、native_binding_support.c:320-325、string.c:958-974 与 string.h:131。
- HASH_BUCKET 软失败：Stream_ReadText / HASH_BUCKET block 两条 BUG 行描述同一结果槽路径，另见 string.c:802-808、hash_set.h:535-538、base/hash_set.c:170-181、dispatcher null 初始化/调用及 StreamSyncFields :472-475。仅在指定 bucket 扩容失败且查询成功时脚本见 true/null；未 fault-inject。
- ReadBytes/ReadText 临时 malloc buffer 是否会被 VM MemoryError 非局部跳转越过 free、NewStreamObject 的非局部 MemoryError 是否越过 File_Open 清理、File_Open path 借用跨分配窗口、ReadText 字节计数的 UTF-8 边界、close failure 后宿主句柄状态及重试安全性、wait-list 入队点，均是有具体核查入口的 TODO；没有将未闭合路径升级为 BUG。

## 独立复核补齐的共享入口与初始快照

HASH_BUCKET block 同时覆盖三个可观察入口：Stream_ReadText 的 helper true 后仍需 :472 StreamSyncFields 查询成功；fs_entry.c:37-54 的 system_fs_read_text_file 在 :47 调相同 ReadTextFromHandle，:49-50 仅在临时 close 失败时把成功改为false，模块 readText :276-284 与 File.readText :419-432 均不检查结果槽。仅该一次短串bucket扩容软失败、临时close成功且余下步骤（含 File 的 :429 RefreshEntryObject）正常返回时，二者亦可true/null，不需要StreamSyncFields。没有追加故障注入或增加缺陷类别。

NewStreamObject :165 忽略 StreamSyncFields 的false；开放句柄 GetHandlePosition/GetHandleLength任一失败时 :202-204 提前退出，构造仍在 :166-167 交付wrapper，position/length尚未写入。此处为同一构造/同步函数合同中的TODO，核查初始查询失败是否应拒绝构造及未知快照如何表示；不将未确定的API政策升为BUG，台账仍31个唯一单元。当前r5源码/strict proof已冻结，native-r2使用旧E884注释版；本次最终源码的直接 native 验证记录已补在末段。

## Root 全量调用核对与规范台账迁移

全首方 C 符号检索经注释/字符串分离后得到21定义、18声明、34直接调用、14descriptor回调绑定及1处finalizer赋值，无未分类的C引用，完整证据报告 system-fs-stream-full-caller-evidence-audit-r1.json（SHA256 842d11cef4879c427fc9ce39b3debd8db57e6aa9af2729399807d0555542e012）。叶台账补齐对应声明和finalizer赋值锚，真实GC间接释放链与wait-list TODO仍分开。File刷新ordinary false被调用者忽略，非局部异常则不能形成正常true返回。

旧 zr_vm_lib_system.tsv 的24条本文件记录逐键迁入31单元专用表：21函数及NUL、UTF-8、close错误三个旧块都有明确映射；原BUG/TODO结论保留或收窄并给证据，旧表其余89行原字节保留。两个Wiki页同步负count/宿主seek、初始快照、字段发布和共享readText结果条件。没有新增复述实现的测试、没有修改共享测试源码或任何程序行为。最终 native、逐项比较和提交验收证据见下段及 Root 的精确暂存 receipt。


## 当前源码的最终构建与提交验收

2026-10-15 Root 在实际 checkout 复用 clang-cl-products-debug，执行 tensor-stream-final-native-r1：build exit 0（23.719 s，14 条 warning 行、0 error）；Module 70/78（原 8 项失败）、Provider 9/9、Fs 7/8（原 C1035）。三组逐项 case lines 与 scalar-stream-vector-final-native-r2 完全相同，不能称为全部测试通过。当前 fs_stream.c 的 1CE5D972 原始字节已在该次构建使用；3216 个保守编译/配置输入、28 个冻结源码验证前后稳定。results SHA256 B2A77E8E1F11D5B5A676B88DFCCE8EFF919D8AF5730F9ED84B54B99D542B6764，comparison SHA256 9A7DA0DC226E726ABA5CBF20D27F03E1ECEA2C713B8675872FAC1DADD8C52917。原始修改前全量构建因其他会话 Parser 输入失败，不能作隔离行为归因；未执行 WSL、allocator/GC/并发/UB 或 shared-plugin 专项。

上述测试到达边界仍按本批 :62–66 的具体语句计，失败后的 Stream callbacks、wait-list finalizer 路线和所有故障分支均无新增执行信用。代码、31 行专用台账、旧表24键迁移、两页 Wiki 和 inventory 的本源码单行组成7条提交路径；Root 的 system-fs-stream-index-r1-receipt.json、root-staged-r1-audit.json 与 independent-staged-review.json 记录精确 patch、三次 cached check 和逐项人工验收。其他会话暂存及 inventory 其余行保留。

本轮自有 lexer/build/test 子进程均已退出。实际释放 0 B；复用构建、缓存、暂存 index 与必要失败日志保留。Fs runtime 当前32文件/104 B（此前28文件/91 B保留、新增4文件/13 B）；此前自动清理审批被策略拒绝，未重试或绕过，故产物清理状态仍未关闭。
