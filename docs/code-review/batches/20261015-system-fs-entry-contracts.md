---
related_code:
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c
  - zr_vm_lib_system/include/zr_vm_lib_system/fs.h
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c
  - zr_vm_library/src/zr_vm_library/file.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_core/src/zr_vm_core/exception/exception.c
  - zr_vm_core/src/zr_vm_core/exception/exception_internal.h
  - zr_vm_core/src/zr_vm_core/exception/exception_try_run.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/object/object_super_array.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
  - zr_vm_core/src/zr_vm_core/string/string.c
  - zr_vm_core/include/zr_vm_core/hash_set.h
  - zr_vm_core/src/zr_vm_core/base/hash_set.c
  - tests/system/test_system_fs_module.c
  - tests/module/test_module_system.c
  - tests/fixtures/projects/gc_fragment_stress/src/main.zr
  - tests/fixtures/projects/native_numeric_pipeline/src/system_io.zr
  - tests/cmake/compiler-project-targets.cmake
  - tests/cmake/debug-metadata-aot-registration.cmake
  - tests/parser/test_type_inference.c
  - zr_vm_library/include/zr_vm_library/file.h
  - zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h
  - zr_vm_core/src/zr_vm_core/base/memory.c
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_values.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c
implementation_files:
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/system/test_system_fs_module.c
  - tests/module/test_module_system.c
doc_type: workflow-detail
---

# System.Fs entry callback 实现契约审查

## 范围与定义清点

本批只覆盖 `fs_entry.c`：8 个内部 helper、34 个 native callback、1 个 `@file` 职责单元，以及 8 个独立失败/资源/生命周期块，共 51 个唯一叶单元。原文件 704 行之后追加了 42 个 `@fn` 契约；源码只改注释，函数定义、非注释 token、行为、签名和 descriptor 均未改。Leaf 分类为 31 BUG、15 commented、5 TODO；多个 callback 行指向共享根因，BUG 单元数不等于独立缺陷数。最终源码 SHA-256：`8D496EF6ECB5F344CA14337696BEEEA3FB775DAB31CE72FE2020206D0AAD1BAA`。本批 producer 未运行构建或测试；Root对同一完整程序内容的实际构建、R10注释身份等价衔接及三组现有测试结果见后文。

callback 入口由 `fs_registry.c` 的函数、方法和 constructor descriptor 提供；native dispatcher 通过保存的 callback 指针调用它们。`tests/system/test_system_fs_module.c` 将 ZR 源码嵌在 C 字符串内运行，人工 fixture 是调用证据而不自动获得本批运行信用。模块表导出名字的测试只验证可见性，不执行模块级 callback。

## 调用和资源边界

模块级 currentDirectory/changeCurrentDirectory/pathExists/isFile/isDirectory/createDirectory/createDirectories/removePath/readText/writeText/appendText/getInfo 在 `fs_registry.c:301-323` 登记。FileSystemEntry constructor 由 `:78` 登记；exists/refresh 在 `:128-130` 登记；File 与 Folder methods 分别在 `:135-186` 登记。actual indirect invocation 由 `native_binding_dispatch_lanes.h:263-277` 完成。全仓符号检索未发现这些 callback 被另一处 C 代码直接调用；模块级 `readText` 还有两个脚本 fixture 调用点 `gc_fragment_stress/src/main.zr:103` 与 `native_numeric_pipeline/src/system_io.zr:11`，这些 fixture 未由本批单独运行。

四个一次性文件 helper 在普通返回路径关闭临时句柄；I/O失败保留主要errno，ReadBytes的Array_New/PushValue普通失败则只保存调用后当时errno，不保证是I/O错误来源；非局部 VM 异常不等同普通 false。ReadText 的字符串物化可抛 MemoryError，跳过 ReadTextFromHandle 的 free(text) 和外层 CloseHandle；ReadBytes 的 Array_New/Array_PushValue 可抛异常，跳过 C bytes 缓冲释放及外层 close。WriteTextToHandle 仅 strlen/write_all，不分配 VM 对象，因此没有该异常清理缺口。WriteBytes 的缺口更窄：仅未物化 RAW_CANONICAL 数组的 Array_Get 进入物化分配时可能 longjmp，跳过 bytes/free 和外层 close；完整链见 BUG 条目。exception_try_run 只恢复 VM roots/GC scope，不遍历 abandoned C 栈；callback 没有这些资源的注册清理。File.open 仅在 owner 发布完整成功后由 FileStream 接管；owner ARRAY ignored-root 单次扩容软失败例外见 BUG 4；owner 发布失败归 fs_stream 批次，构造异常也可越过本地 close。

system_fs_make_entry_array 仍需沿实际 native lane 核查局部数组/当前 entryObject 的参数保活、GC safepoint 后字符串存储稳定性及连续构造窗口；未证普通对象悬空或重定位。另已闭合资源 BUG：ListDirectory/Glob 成功取得 C 列表后，NewEntryObject、Array_New 或 Array_PushValue 的 VM 分配若抛 MemoryError，会跳过 system_fs_folder_list_common:611 或 Folder_Glob:647 的 File_List_Free；exception recovery 不遍历 abandoned C 栈，dispatcher 只恢复 VM roots/GC scopes，未登记 C-list cleanup。普通失败返回路径仍执行显式 free。列表中每个 NewEntryObject 还经过共享 WriteStringField，短串 HASH_BUCKET 单次扩容失败可把未初始化 tagged value 送入 setter，属于未定义行为而非稳定的缺字段对象。

File/Folder copyTo 与 moveTo 先完成宿主副作用，再构造目标 wrapper；包装失败不回滚副本/移动。File 版本在构造前调用 RefreshEntryObject 并忽略普通 false，因此源 fileInfo 可能旧；非局部异常仍会退出 callback。Folder_MoveTo 不刷新旧 receiver。目标 path 在后续 wrapper 分配中继续借用，pin/GC 地址窗口仍为各 callback 的 TODO。所有目标 File/Folder 对象都走共享 WriteStringField，一次 HASH_BUCKET 软失败可导致未定义行为，见字段发布 BUG。

多项 File/Folder 变更 callback 忽略 RefreshEntryObject 的普通 false，仍可报告成功且保留旧 fileInfo；这与非局部异常不同，后者会退出 callback。refresh 中写入 SystemFileInfo 字符字段也经过共享 WriteStringField，其初始化失败后的 setter 使用属于 UB；不把普通 false 解释成此 UB，也不承诺稳定缺字段。各受影响接口的公开刷新策略仍列 TODO；短串结果槽 true/null 是另一条 fs_stream.c 路径。

## 可证 BUG 与待核 TODO

1. **CurrentDirectory 的 true/null。** native dispatcher:1839 预置 null；规范化 cwd 短且未驻留、单次 HASH_BUCKET 扩容软失败时，CurrentDirectory 调用 void Value_SetString 后仍 true，因此脚本可观察 true/null。短串缓存路径见 native_binding_dispatch.c:1303、native_binding_support.c:320、string.c:958、hash_set.c:170。只限定该单次分配失败且其他路径成功；本批无故障注入。changeCurrentDirectory 传当前路径不保证 cache miss。
2. **readText 的共享 true/null 与异常清理。** module ReadText:281 与 File_ReadText:425 经 system_fs_read_text_file:47 调 ReadTextFromHandle，不检查结果槽是否仍为 dispatcher 的 null；短串 cache miss 且单次 HASH_BUCKET 扩容软失败时可 true/null，软失败链归 fs_stream 批次。独立资源 BUG：长串物化若抛 MemoryError，会越过 fs_stream.c:351 的 free(text) 和 fs_entry.c:49 的临时句柄 close；本批无故障注入。
3. **共享字符串字段 setter 的未定义行为。** 窄条件为短串 cache miss、跨驻留表阈值，且自定义 allocator 仅该次 HASH_BUCKET 扩容失败；fs_common.c:151-169 的 Value_SetString 不初始化 fieldValue 后，void Object_SetFieldCString 仍读取该槽，行为未定义，不能断言稳定的 missing-field 对象。消费点包括 GetInfo:320/330、Entry_Constructor:347、Entry_Exists/Refresh:357/368、system_fs_make_entry_array:160、File/Folder copy/move 的 :535/:558/:666/:685，以及 RefreshEntryObject 的元数据字段写入。共享 allocator 证据归 fs_common 批次；本批无故障注入。
4. **File.open owner 发布。** File_Open:394 对 NewStreamObject 非 null 直接 FinishObjectResult:399；owner key 的一次 ARRAY ignored-root 扩容软失败仍交付 wrapper 时，本层不 close 句柄，之后 stream 无法经 owner 字段释放 native record/handle。触发点归 fs_stream 批次；本批无故障注入。构造期间的其他非局部 VM 异常同样会跳过本地 close。
5. **一次性 ReadBytes 的缓冲与句柄清理。** fs_entry.c:68 调 ReadBytesFromHandle；该 helper 的 Array_New:290 或 Array_PushValue:299 若抛非局部 MemoryError，会越过 fs_stream.c:292/:300/:305 的 free(bytes) 和 fs_entry.c:70 的 CloseHandle。普通 null/false 返回仍走显式清理。exception_try_run.c:10-27 与 native_binding_dispatch.c:1760-1777 证明恢复只处理 VM roots/scopes、不清理 abandoned C 栈；本批无分配故障注入。
6. **RAW_CANONICAL WriteBytes 的临时资源。** File_WriteBytes/File_AppendBytes:491/509 经 system_fs_write_bytes_file:118 到 WriteBytesToHandle:379 的 Array_Get。RAW_CANONICAL 状态由 object_super_array.c:1870 的整数写入路径建立；execution_dispatch.c:5715 是优化整数追加入口，编译器 compile_expression_values.c:539/553/580 将数组字面量编译为 CREATE_ARRAY/SET_BY_INDEX。fixture tests/system/test_system_fs_module.c:753 只证明这是合法数组输入，不证明该对象当前为 RAW_CANONICAL。失败链严格从 Array_Get (`native_binding_dispatch.c:1542`) 到 Object_GetValue (`object.c:2007`) 的 dirty raw-array materialization：EnsurePairPool (`object_super_array_internal.h:486`) 调用 HashSet GcMalloc (`hash_set.c:119`)，MemoryError 在 `memory.c:46` 抛出并由 native binding 非局部退出 (`native_binding_dispatch.c:1760`)，跳过 fs_stream.c:403 的 bytes free 与 fs_entry.c:120 的 CloseHandle。exception_try_run.c:10 明确不遍历 abandoned C 栈；普通 false 与非 RAW_CANONICAL 路径仍执行清理。本批无故障注入。
7. **目录列表物化时跳过 File_List_Free。** Folder entries/files/folders 共用 system_fs_folder_list_common（定义 :595）；helper 在 :607 调用 ListDirectory 并于 :611 释放列表；Glob 在 :643 取得列表并于 :647 释放。平台 list 已成功后，system_fs_make_entry_array 的 Array_New/NewEntryObject/PushValue VM 分配抛 MemoryError 会 longjmp，跳过 File_List_Free；exception_try_run.c:10 不遍历 abandoned C 栈，exception_try_run.c:18 只恢复 VM roots/GC scope，native_binding_dispatch.c:1760 进行非局部退出。平台 ListDirectory/Glob 的普通失败清理不属于此 BUG。

其余待核 TODO 包括：Entry constructor 的 VM path 在 receiver resolve/populate 分配间被复用；File.open fullPath 在 NewStreamObject 分配后继续使用；File/Folder CopyTo/MoveTo targetPath 跨 VM 分配继续使用；entry array 沿 native lane 参数保活、GC safepoint 后字符串存储稳定性及局部 roots；File.open wrapper 失败时 CloseHandle 自身失败后的 host handle 状态；RefreshEntryObject 普通 false 是否应改变 mutation 成功/快照策略。非局部 MemoryError 跳过临时缓冲、句柄及目录列表清理已由本批 BUG 的 longjmp 链证明，不再列为 TODO。Folder_Glob省略recursively的公开默认已由:639–641传ZR_TRUE到fs_common.c:323–325省略分支确认true；descriptor只限制1..2元。原默认TODO据此闭合为commented；现有用例显式true，不提供省略参数的运行信用。

## 测试入口与实际断言边界

- `tests/cmake/compiler-project-targets.cmake:187-198` 定义 `zr_vm_system_fs_test`；`tests/cmake/debug-metadata-aot-registration.cmake:862-870` 将其作为 `system_fs` suite 注册。测试文件 `main` 在 `test_system_fs_module.c:1522-1529` 登记 8 个 Unity tests。
- metadata test `test_system_fs_module.c:625-690` 读取 module info、File/Folder descriptor 和 FileStream FFI 字段；它不执行模块级文件 callback。
- path object runtime test `:733-854` 通过 ZR 脚本真实断言 FileSystemEntry constructor/exists、File/Folder create/delete、File 文本和字节读写、copy/move、entries/files/folders/glob（`true` 明传）、refresh 与快照字段；脚本路径调用在 `:736-785`。它不包含移动 GC 压力、allocator failure 或文件内容 NUL。
- copy result test `:873-925` 拆开断言目标对象的 exists/readText 调用，源码调用点 `:882-885`。
- stream_modes test :944-1043 在 File.open 场景中继续运行 stream 方法；修改前冻结 suite 为 7/8。唯一失败是 C 测试断言 test_system_fs_module.c:1035，stderr 为 takeReader:7 invalid cached native member callable。file.open("w+") 在 :965 到达；脚本 takeReader(stream) 在 :976 失败，所以后续 stream.readText(-1) :978、追加/bytes/using 分支没有执行。此失败不算 File_Open close/GC/failure 覆盖，也不算 ReadText success 覆盖。
- missing-file test `:1060-1100` 执行缺失路径 `File.open("r")` 并捕获 IOException，覆盖常规 OpenHandle 失败路径，不覆盖构造失败后的 close 失败。
- test_module_system `:3144-3146` 对 currentDirectory/changeCurrentDirectory/pathExists 只检查导出名；仓内未发现这些兼容 callback 的实际行为调用。getInfo 也没有本批可运行的专属调用断言。
- 项目 fixture `gc_fragment_stress/src/main.zr:103` 和 `native_numeric_pipeline/src/system_io.zr:11` 提供 module readText 的静态调用点；本批没有运行这些项目/fixture runner，不能给它们执行信用。

本批生产者未运行测试。Root 的 tensor-stream-final-native-r1 使用原始 fs_entry.c SHA-256 84F4FC4AD16CD55B491F5C56965E1D860D26C3AF6A4C8261617DBE23CB195B49，是修改前组合基线：build exit 0，Module 70/78（既有 8 项失败）、Provider 9/9、Fs 7/8（测试断言 test_system_fs_module.c:1035；stderr takeReader:7 invalid cached native member callable）。结果不表示注释后验证，也不覆盖本批 OOM/GC/allocator 路径；最终证明与复用 suite 由 Root 管理。

## 定义级契约与分类

leaf 的唯一键按 `(file, unit, kind)` 记录：本文件、8 个内部 helper、34 个 callback、上述独立问题块与 file-level 职责均逐项覆盖。decision 保留现存可证 BUG/TODO；callback 行的 `commented` 仅说明当前 Doxygen 事实和对应共享缺陷入口已写清，不授予测试通过信用。`no-comment` 不用于公开 callback 或 helper。批次与台账的测试边界必须按本节具体调用与断言理解，不能以 suite 名、descriptor 存在或 fixture 调用点替代运行证据。

## Root 独审返回语义与静态测试补齐

File_ReadBytes 的一次性 helper 普通false（含Array_New/PushValue普通失败）在:474–475转入RaiseErrnoIOException，不能全部称作前置校验false。File_WriteBytes/AppendBytes在:491–492/:509–510把helper普通false转入同一IOException路径，数组非整数/超0..255的元素会先由WriteBytesToHandle设置EINVAL；三个公开Returns及叶合同均补齐，非局部MemoryError仍与普通false分开。

tests/parser/test_type_inference.c:7918–7957的现存测试字符串在:7930引用getInfo/currentDirectory，实际只Parse、编译导入并在:7952–7953断言ExpressionType_Infer为INT64，没有执行callback。该文件已纳入related_code；叶callers明确标静态消费者，本批没有运行该Parser测试，也不给兼容Fs callback行为信用。

## File.open 模式长度与错误转换

File_Open:380/385用8字节modeBuffer复制首NUL前缀；fs_common.c:308–310在前缀超过7字节时普通false，尚未调用OpenHandle。可容纳但非法模式（空串/q等）由file.c:686–740的file_parse_mode设EINVAL，OpenHandle:1375返回false，再由File_Open:389–390进入RaiseErrnoIOException；这不是只有宿主open失败才报错。非string模式仍由ReadString抛TypeError、缺参用r。公开Parameters/Returns、叶合同及owner移交的完整发布前提已补齐，无模式错误/长度/NUL专项执行信用。

## Root 规范迁移与当前源码验收

旧System聚合表的48个FsEntry键逐项迁到本批51键；42函数去掉旧implementation后缀，6个旧TODO块映射到具体借用/root/default单元，5条未闭合TODO保留，原Glob默认项已由完整调用路径闭合为commented，新增文件职责与2个独立块。旧表其余41行原字节保留。51键为31 BUG、15 commented、5 TODO；全清单无未审查单元，BUG单元数不等于独立缺陷数。

Root亲读原704程序行、42个EOF文档级合同与完整51行台账。system-fs-entry-final-r10-comment-equivalence.json SHA256 2929a3d4b0afa5a40f5fc8ce7251564923e082b7ff2ff73bd57228927cd356f6：4693程序token、2 lexer全部等到退出，种类/拼写/词法flags及原物理行一致。全仓调用报告 SHA256 2c290770dfdf075980f7ac21da6ccecca353b0c7f6029e3b5e6cac4afd06dfdc：42定义、34声明、34 descriptor绑定、32直接调用、0未分类C引用；全部定义/声明/调用/指针坐标补入叶证据。descriptor constructor由三个type复用不等于三个直接C caller。system-fs-entry-nonlocal-resource-callpath-audit-r1.json SHA256 cf9dd851144e8445579765c12feba80121b5a5b9e048a32bff2a02aa01c4a54c固定19份证据源码与6类静态闭合链，R9 caller/resource-source-bridge核对最终源码及其余证据身份，区分RAW合法来源、物化消费者、ordinary false及非局部退出，不赋予故障注入信用。

R3注释阶段的实际 checkout 验证 math-config-fs-entry-final-native-r1：build exit0（75.453 s，105条warning、0 error）；Module70/78、Provider9/9、Fs7/8，三组逐项case lines与tensor-stream-final-native-r1完全一致，保留8个Module失败及Fs断言1035的takeReader cached native member失败。results SHA256 de826a391eccc12ca129ff63a853b4d7ceddafa84cb913554edcf126c68a4384；comparison SHA256 032bf97f9a3510a41d86f468022a95bcdd72ceaacf96de67858e12dea9181694。3216输入及31冻结源码运行前后稳定；修改前全量构建受其他会话Parser输入阻断，不能作隔离行为归因。无allocator/GC/并发/UB/WSL/shared专项或未到达callback执行信用。

本批同时校正已交付Stream的6条既有单元和private fs_internal.h的4个既有声明合同；分别仍31键、73键且不重复累计。两页Wiki更正File.create既存文件行为、C-string文本和owner/finalizer保证，语句或API签名均未改。Root与独立reviewer的本批13路径精确暂存审查、三次cached check及He-Jiahui提交身份另见system-fs-entry-index-r1-receipt.json、root-staged-r10-audit.json与independent-staged-review.json。新交付51单元，累计1533。

自有lexer/build/test进程已退出。本批实际释放0 B，Fs夹具保留36文件/117 B、原32文件/104 B未改；构建/缓存/index及必要失败证据保留。此前自动清理被策略拒绝，未重试或绕过，产物清理未关闭。

## 失败状态、副作用与注释R9验证衔接

公开六个text/bytes写入callback及两个一次性write helper现在明确：宿主短写后续报错或零进度EIO可留已写前缀，IOException/false不回滚。bytes完整元素校验发生在首次WriteHandle前，但一次性File helper在此前以w/a打开：writeBytes对非法元素也可能先截断旧文件，appendBytes不追加非法内容却可创建缺失文件。text的w打开同样可在后续失败前截断。证据是fs_entry.c:114/118、fs_stream.c:110–125/:373–403，file.c:686–740/:1361–1403；不授予短写、无效元素副作用或close故障执行信用。

四个CopyTo/MoveTo明确宿主操作本身失败亦不保证原子或回滚。file.c:772–774/:847–849先删除overwrite目标；:793–831打开/写目标失败只close，:851–879递归失败保留已建目录/子项；:1259–1269在rename失败后Copy(true)&&Delete(source)，copy失败不删源，copy成功源删除失败则可能两边仍有内容。module/Folder CreateDirectories和File_Create说明递归失败保留已建父目录，file.c:1187的close失败亦保留新文件；Folder_Delete递归失败后此前删除的子项不恢复（:882–903）。此前R7对此类失败状态只补充契约，不新增unit；R8新增确认的两个窄缺陷家族另见下节，仍归现有函数键，不重复累计。

R10 source SHA256 8d496ef6ecb5f344ca14337696beeea3fb775dab31ce72fe2020206d0aad1baa保持原1528物理行，只修改已有EOF文档，R8相对R7修订5行、R9再修订1行，4693程序token及全部词法flags/原程序行严格一致，proof SHA256 2929a3d4b0afa5a40f5fc8ce7251564923e082b7ff2ff73bd57228927cd356f6。上方actual native实际使用R3 source31f502，并未直接编译R10注释字节；system-fs-entry-final-r10-native-token-bridge.json SHA256 b0cebedf01b0a1fa1929b6deeb4cce6f3bd6598d58a91df16f3e44787edd8b85逐一固定实际results、R3/R10原始身份、完整程序摘要及token坐标一致。未重复运行相同程序的三suite，不为新文字增加执行覆盖。R10 caller/resource bridge SHA256 4156c7da25578ac6456da7d72a654b91236ef7bb10b941128f3062258925c3d2保持全部静态锚及19源码证据身份；sharedStreamR6和privateHr2仍是该native直接使用的原字节。

File.open的w/a/x模式在宿主open成功后已经可能截断/创建文件，随后append seek失败、wrapper普通NULL或非局部异常都不恢复内容；wrapper普通NULL只尝试CloseHandle（entry389–397，file.c722–736/1385–1400）。当前R7第18条EOF修订还关闭Glob省略参数默认的过期TODO：当前实现确定true，缺少省略调用专项执行与“默认尚未确认”不同。旧48键的6条TODO中只有此key凭完整调用路径转commented，其余5保留；关闭证据system-fs-entry-final-r7-default-resolution.json SHA256 db89257767e77d80cfce27d62811aa47d89d566622d5d5e5fe3b498d8778dc44，不作无依据降级。

## R8 宿主复制与移动边界复核

实际复制普通文件内容的helper在file.c:823已标success，:825–832忽略两个CloseHandle返回值；Copy（含目录子文件）及Move的copy回退因而可能成功而close失败。rename直接成功分支没有该helper。overwrite目标删除早于source open（:772–781），父目录创建早于target open（:786–805）；后续失败可能只留下父目录、让旧目标已删但新目标尚不存在，亦可留下已写前缀/部分树，均不回滚。ReadBytes私有Returns还区分数组构造普通false的非I/O errno来源。

8. **同路径overwrite移动会删除源。** File/Folder callback :552/:682将source与target传Move；:1250–1251的NormalizePath后，若两者为同一现存路径且overwrite=true，:1259–1261先删target即source。限定删除成功且没有外部重建，:1264 rename与:1268回退Copy在:1228–1231均因源不存在失败，callback :553/:683抛IOException，源已删除。完全相同输入即可满足，不推断所有大小写/符号链接/hardlink别名。本批只以完整调用路径标BUG，无同路径破坏性运行。

9. **目录复制把新建子目标纳入自己。** 现存源目录a、尚不存在的目标a/b，:851先建立b，:852经ListDirectory:1290和:515–567枚举a，新增b被列入；:865–866再把b复制至b/b，每层重复先建后枚举。初始创建/枚举成功时可递归增长并留下嵌套部分树；Move的rename失败后Copy(true)回退同样可达。file_join_into:658–665另有snprintf截断误判成功，不能承诺该递归一定于:860路径拼接失败终止。本批无递归增长专项运行，目标自身子树作为当前禁止使用的输入边界写入Folder两个函数合同。

R8边界报告system-fs-entry-final-r8-transfer-boundaries.json SHA256 7753a842f7e055c5d58a5487f6f4b3e22f3d8bd04f575d8203c1699f04e3c1cf；上述新根因归既有31条BUG函数记录，不新增51键或累计数。已交付宿主聚合表zr_vm_library.tsv里的Move/Copy helper旧no-comment结论须在紧接的宿主契约更正批同步，本13路径批没有把这些历史记录算作已覆盖新边界。R8历史proof 50399a951936cef34955f6d93b74ca0521a03e1582fa37ff50681c9613e34d58；actualnativeR3→R8完整4693token、flags与原程序坐标一致，R7后只修订5条既有EOF文档行；没有新的执行信用。

R9仅在File_Open的Returns :1158明确OpenHandle在宿主open成功后append seek-end失败也返回false并转IOException（file.c:1394–1400，entry:389–390）；Constraints原有不回滚副作用仍适用。R9最终source ff21ee07978cdca1069fdef4f77b89dcd72e0c7ad23694d3611b10ec7fef97ba；strict 97c97647a9211ad523848bea4bf5f62df07b0d36d673911de63894ae392e36b8；native bridge 9fa8a1935934285546f80fe60309d879027072422564438cd6de3e6176e230f5；仍为51键31BUG15commented5TODO，不新增执行覆盖。上节R8“五行细化”及transfer报告为保留的修订证据，R9为该轮冻结身份，当前最终源码以末尾R10身份为准。

## R10 词法路径关系矩阵与祖先覆盖删源

本表限定源存在、路径规范化成功、无外部重建/改名/cwd变化；关系针对两个NormalizePath实际输出的组件边界，既不按原始拼写，也不提供realpath、symlink、hardlink或大小写折叠保证。两类wrapper的构造/这四callback不校验源实际类型；Copy按stat分派（entry:340–350/common:486–570，file.c:1228–1239），故“实际目录”的递归限制也适用于File wrapper。

| 关系 | Copy | Move |
| --- | --- | --- |
| 规范化字节相同 | 源存在后:1233–1234直接true，overwrite真假均不删目标；后续wrapper失败仍可令callback失败 | overwrite=false因目标存在而失败；true删除源本身，再rename/Copy失败 |
| 目标是源严格祖先目录 | false因现存目标而失败；true先递归删祖先，源及兄弟均被删；文件打开源失败，目录分支可能重建空target后列源失败 | false不删；true删祖先连源与兄弟一起删除，随后rename/Copy失败 |
| 目标是实际源目录严格后代 | false且目标已存在则失败；否则先删允许覆盖的旧子目标/建立新目标，再列源，把新目标继续复制到其子目录，反复递归 | false且目标存在则失败；否则尝试rename，仅在rename失败后的Copy(true)回退触发同一递归；true可先删旧子目标 |
| 无词法重叠 | false遇现存目标失败；其余按stat复制，旧目标删除、parent-only、部分输出及ignored close限制仍适用 | false遇现存目标失败；其余rename成功即返回，否则Copy(true)&&Delete(source)，不回滚 |

10. **覆盖祖先目标会删除源与同树兄弟。** 充分条件是overwrite=true、现存target为source严格祖先目录、递归删除成功且无外部重建。Copy先在:1228 stat源，再于普通文件helper:772–783或目录helper:847–853删目标即删源；Move:1259–1269同样先删祖先再rename/Copy失败。递归删除链:882–903；File/Folder CopyTo/MoveTo的:529/:552/:663/:682将失败映射IOException（:530/:553/:664/:683）。这是既有BUG函数行内的新确认条件，不新增单元；未作破坏性运行。Move在删除overwrite目标前也不预验源存在，缺失源仍可能导致现存目标先被删除。

当前R10 source 8d496ef6ecb5f344ca14337696beeea3fb775dab31ce72fe2020206d0aad1baa；proof 2929a3d4b0afa5a40f5fc8ce7251564923e082b7ff2ff73bd57228927cd356f6；actualnativeR3→R10 bridge b0cebedf01b0a1fa1929b6deeb4cce6f3bd6598d58a91df16f3e44787edd8b85；caller/resource bridge 4156c7da25578ac6456da7d72a654b91236ef7bb10b941128f3062258925c3d2。R9→R10只扩写4条已有EOF文档行，51键31BUG15commented5TODO不变，无程序/API/测试修改或新增执行信用。上节R8/R9均为历史修订身份，当前以此R10为准。
