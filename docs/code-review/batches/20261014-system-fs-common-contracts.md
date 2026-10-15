---
related_code:
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_core/src/zr_vm_core/string/string.c
  - zr_vm_core/include/zr_vm_core/hash_set.h
  - zr_vm_core/src/zr_vm_core/base/hash_set.c
  - zr_vm_core/src/zr_vm_core/exception/exception.c
  - zr_vm_core/src/zr_vm_core/exception/exception_internal.h
  - zr_vm_core/src/zr_vm_core/exception/exception_try_run.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/object/object_super_array.c
  - zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h
  - zr_vm_core/src/zr_vm_core/base/memory.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
implementation_files:
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/system/test_system_fs_module.c
  - tests/module/test_module_system.c
doc_type: workflow-detail
---

# System.Fs common helpers 契约审查

## 范围与清点

本批覆盖 fs_common.c 与 fs_internal.h；fs_entry.c、fs_stream.c、Core、Library dispatcher 和测试仅作只读调用链证据。coverage leaf 有 73 个唯一 unit key：fs_internal.h 44 项（文件说明、include guard、2 个宏、stream 结构及 6 字段、33 个共享声明），fs_common.c 29 项（文件职责和 28 个函数定义）。本批未发现未登记的静态数据表或独立配置块；两个字符串 setter 缺陷、path 借用 TODO 和无调用 getter 都记录在所属函数单元，未重复拆成同一语义的伪单元。

表内仍73键；FsEntry复核后四个Stream helper声明合同同步为58 commented、2 no-comment、7 BUG、6 TODO。新增BUG结论是既有ReadBytes/WriteBytes声明单元的证据升级，不新增单元或重复缺陷数；path借用、getter用途和errno等疑点仍具体TODO。

## 调用链与契约

fs_internal.h 是 fs_common.c、fs_entry.c、fs_stream.c 的直接共享头。公开 entry callbacks 通过 common helper 构造 FileSystemEntry/File/Folder 与 SystemFileInfo；stream callbacks 通过同一组字段访问器读取隐藏 owner、同步 position/length/closed 和执行字节/文本读写。StreamFinalize 实际赋给 FileStream 的 scanMarkGcFunction；它单独从 wrapper 查找 owner 并尝试关闭、释放记录，StreamCloseInternal 只由 Stream_Close 路径调用，不是 finalizer 的共用实现。

对象快照使用 ZrLibTempValueRoot 跨 VM 分配保活并重取对象。当前普通 OBJECT 晋升保持地址稳定，因此 NewEntryObject 曾有的 receiver 移动风险不再成立。PopulateEntryObject 的 originalPath/fullPathOverride 可能来自 VM 字符串，并跨 QueryInfo、SystemFileInfo 构造及父目录递归分配继续读取；本批未闭合各 native lane 的参数 pin/字符串存储地址与完整可达 GC 窗口，故仅保留具体 TODO，不断言已产生悬空地址。RefreshEntryObject 则在读取 fullPath 和 QueryInfo 后创建 info 对象，再由 WriteObjectField 进入 setter；object receiver 当前稳定，Setter 对 object 先固定 value。它返回本次查询快照成功不等同于 void setter 一定更新 fileInfo 字段。

ReadStringArgument 委托 CallContext_ReadString，类型错误沿 VM 错误路径；ReadOptionalStringArgument 缺参复制默认值，buffer 太小仅返回 false 且不覆盖输出 buffer。ReadOptionalBoolArgument 只接受 bool，类型不符为普通 false，入口已写默认值；ReadOptionalIntArgument 委托 ReadInt，需按其类型转换/VM 异常契约处理。不要把这些结果统一写成 IOException。RaiseErrnoIOException 捕获进入时的 errno；stream caller 先求值 GetStringField(path)，是否覆盖原 errno 尚未证实，沿 Object_GetFieldCString 核查并记 TODO；RaiseIOException 用 512 字节栈 buffer 格式化消息，再创建并根住 IOException。

以下记录可达 BUG 链及有核查入口的 TODO；本批没有执行 allocator 故障注入：

1. WriteStringField 的短串 cache miss 跨 HASH_BUCKET 负载阈值时，若自定义 allocator 仅这次非零 bucket 扩容返回 NULL，string cache 移除新短串并返回 NULL。ZrLib_Value_SetString 在此返回路径不初始化局部 fieldValue；随后 Object_SetFieldCString 的 inline lane 读取 value->isGarbageCollectable 并可进入 pin 路径。system_fs_set_message_field 以相同 helper 顺序构造 IOException message，也把未初始化 fieldValue 传给 void setter。这是限定单次 bucket 软失败的静态可达缺陷，不扩大为任意 OOM。
2. NewStreamObject 在句柄已打开并分配 owner 记录后设置 GC callback 和隐藏 owner 字段。owner 字段由 void setter 写入，失败不反映到返回值；File_Open 仅在 NewStreamObject 返回 NULL 时关闭 handle。若仅 owner 字段写入软失败，wrapper 仍可发布；GetStreamData 后续因隐藏字段缺失返回 NULL，StreamFinalize 也无法取得记录，malloc 记录与宿主句柄都失去释放路径。没有 owner 指针包装器自带析构可补救此路径；本批未做故障注入。
3. PopulateEntryObject 的 path C-string 借用窗口尚未完成 lane 与可达 GC 条件核对，因此是 TODO，不是已证缺陷。ordinary OBJECT receiver 的移动疑点已由当前 Core 晋升语义排除。

WriteBytes/ReadBytes helper 以 byte array 和逐项范围校验处理字节；ReadTextFromHandle 按字节消费后通过 C-string SetString 创建 VM string，所以首 NUL 后缀截断且不验证 UTF-8；WriteTextToHandle 按 strlen 计算长度，只写首 NUL 前缀。I/O 已消费/写出的字节无法回滚，VM 结果构造失败不恢复句柄位置或已写前缀。FileStream callbacks 在底层操作后再同步宿主 position/length，若同步查询失败会报告错误，但之前的 I/O/seek/resize 副作用不回滚。

## 测试边界

本批只读核对了 tests/system/test_system_fs_module.c 和 tests/module/test_module_system.c，未新增实现镜像测试；Root 的实际构建与既有 suite 执行另列于验证结果。模块 metadata 用例 625–714 检查 exported fields 与 FFI hidden fields；path_objects 用例 733–854 和 copy result 用例 873–925 通过公开脚本触达路径对象、字节 I/O 与快照；missing-file 用例 1060–1102 检查 IOException 公开路径。它们都不单独断言 common helper 的 setter/GC 分配失败。

此前 Fs 公共叶基线 Fs8 为 7 PASS / 1 FAIL；stream_modes 在 takeReader 处因 cached native member callable 失败（测试源 1035），其后 readText、SetLength、write/read bytes 与 using 语句没有执行信用。该段属于先前 Fs 公共批次的历史基线，最终重跑另列。Fs common 修改前支持库静态构建为 exit 0、5.515 秒；修改前完整测试 target 构建曾被外部 Parser 缺少 ZR_INSTRUCTION_USE_RET_OPERAND_EXTRA 阻断，因此该次 pre-edit 没有启动 suite，不能与最终重跑混淆。静态构建结果摘要 ee96dd959bd6b10935d087100f58484589af82154dfd0b3d6cfedd76b007c302，比较摘要 144feb913cc90c43a0f91b0ca60c9eaa66dfe092146c2571bc83b7814c1c0c7c。

## 修改与验证状态

先前已提交批次冻结源：fs_common.c SHA256 eba198dd90fcfc98dbf8c173be4c4cfbc274b26cadc4b14e0779e28b45998a0a（702行）；fs_internal.h SHA256 fb7f1b155f12e91c2625547225ea87c7a731d9b658690cd2f8a4049fd8fdd8e6（631行）。严格 system-fs-common-final-r7-comment-equivalence.json（SHA256 30539a784bc481dd0d46df741731f7a61b2cce99b2c5147053f47772ec83dd54）通过4119个非注释token种类/拼写/StartOfLine/LeadingSpace比较，4个lexer子进程全部等到退出，C token原行号不变。该段记录先前提交身份；新header原始字节、r2词法证明及当前构建结果见下方追认。

ReadText 另有独立于 NUL 截断的软失败 BUG：stream.c:347 -> native_binding_dispatch.c:1303-1315 -> native_binding_support.c:320-324 -> string.c:767-818。未驻留短文本跨驻留阈值且仅一次 HASH_BUCKET 扩容申请软返回 NULL 时，SetStringObject 因 NULL 不写 result。dispatcher.c:362 预置 null；helper:352 仍 true，File 包装器 entry.c:47-55/:425-429 在 close 成功后仍 true，Stream:471-475 在同步成功后仍 true，脚本正常得到 null。完整静态路径支持该结论，没有故障注入，不概括硬 GC OOM。

finalizer 在 stream.c:257–264 忽略 CloseHandle 结果并 invalid/free(data)，仅承诺尝试关闭。原ReadBytes/ReadText malloc缓冲及NewStreamObject尚未包装句柄的非局部清理TODO现已闭合为BUG：exception.c:545–570/exception_internal.h:18用longjmp，exception_try_run.c:10–27仅恢复VM roots/scopes，无C malloc/fd/list清理注册；callback后置cleanup不执行这些局部资源释放。WriteBytes另限定非空、未物化的合法RAW_CANONICAL且当前物化需新增pairpool，相关VM分配异常非局部退出跳过free；普通false路径仍显式清理。无故障注入。

Root 全仓候选扫描不按扩展名筛选，剔除注释/string/char 后核得38个函数各唯一实现、257处direct call；只有 StreamFinalize 在 stream.c:159 有非调用函数指针赋值，GC OBJECT 分支见 gc_object.c:1008-1014。gc_sweep.c:71-72 是另一个泛型派发位置，但当前 waitToRelease 入链路径尚待核，不能借此证明 FileStream 必经该队列；gc_mark 的 NATIVE_DATA 分支不是普通 FileStream OBJECT 的入口。GetBoolField 仓内无直接 call/裸引用，不推断仓外不可达。全部函数caller列已列完整实际坐标，字段按owner使用列全；审计元数据 system-fs-common-full-caller-evidence-audit.json（SHA256 c48ad120f8396399fe2c06be4101552759a8d5e2c0c2f2a0f9df9de719cc16e7）。

先前提交的实际 checkout 验证 support-common-final-native-r2-results.json（SHA256 5fa0fdb3ef91eefc45022f086f2806134696b69efed8e0121ab0e01d98aafe5d）：build0/5.047秒；module70/78 exit8、provider9/9 exit0、Fs7/8 exit1，所有case行分别与Complex最近基线/Fs基线一致。3216输入及13冻结源码在运行中稳定；对比 81f1f28267e260a31f251aaa6008bdaabfaddb8f3ff11644a021335b66e8a361。pre-edit完整build1来自外部Parser宏定义缺失，当前Parser已变且阻塞解除，不能隔离归因注释。Fs stream_modes仍在takeReader处失败，后续readText/setLength/bytes/using没有执行信用；无新增allocator/GC/数值极值/并发/WSL信用。删除旧聚合66个key并保留其余113行原字节；本批73key为唯一canonical叶。共享构建/缓存/索引复用，累计20个65B Fs生成夹具保留，本批释放0B；此前自动清理拒绝未绕过、任务未关闭。

Root 先前提交前校正 h:257/279/320 的错误返回描述与两个 caller References，并在27个非void Returns原行补@return标记：mode 在 File_Open，optional int 仅两个 read count，seek/length 直接 ReadInt。这些注释修订晚于 native-r2，r7 与 r5 的非注释摘要及全部token坐标一致；system-fs-common-final-source-native-token-bridge-r2.json（SHA256 dd45c61251bb9eb99319b2436134fc6eb6f8607cb0828f6c26213ee5f76f53dc）固定实际测试源码与最终源码身份。未为相同程序语义重复运行 suite，不将注释文字修订另算执行覆盖。


## FsEntry 审查闭合后的声明合同追认

本次仅更正 fs_internal.h 四个 Stream helper 的已有声明合同、对应73键台账的四行及上文旧未决清理段。所有声明签名、程序token物理行、unit keys与其余69行原字节保持；不重复累计73单元。header单独严格词法证明与本次最终native由Root在FsEntry同批验收中记录；旧r7/source fb7f1b15 及旧构建结果保留为历史证据，不当作新header原始字节的构建信用。完整非局部链和RAW数组构造入口见 [FileStream追认](20261015-system-fs-stream-contracts.md)。

## 当前私有头文件与共同构建身份

当前fs_internal.h SHA256 c2684f8bbd837f38a1d470bcccf1b6f507eb575b1b01071892df22d9b6ad6e88；system-fs-internal-nonlocal-final-r2-comment-equivalence.json SHA256 54cdddd33169c549b8b6cd8bf1ad48b5e3946833d588bf9686f4f53c4e40fdc1证明660程序token、2lexer、原token物理行及声明签名一致，含NewStreamObject openResult只有完整owner发布才转移所有权的限定。fs_common.c仍eba198dd90fcfc98dbf8c173be4c4cfbc274b26cadc4b14e0779e28b45998a0a；r7仅用于该未变C源，旧fb7 header不是当前头文件身份。最终实际 checkout 验证 math-config-fs-entry-final-native-r1：build exit0（75.453 s，105条warning、0 error）；Module70/78、Provider9/9、Fs7/8，三组逐项case lines与tensor-stream-final-native-r1完全一致，保留8个Module失败及Fs断言1035的takeReader cached native member失败。results SHA256 de826a391eccc12ca129ff63a853b4d7ceddafa84cb913554edcf126c68a4384；comparison SHA256 032bf97f9a3510a41d86f468022a95bcdd72ceaacf96de67858e12dea9181694。3216输入及31冻结源码运行前后稳定；修改前全量构建受其他会话Parser输入阻断，不能作隔离行为归因。无allocator/GC/并发/UB/WSL/shared专项或未到达callback执行信用。

只更正已有73键中的4个Stream声明行，其他69行原字节保留；现58 commented、2 no-comment、7 BUG、6 TODO，无新增/重复73单元。Root原有FileStream/finalizer软失败和errno TODO仍保留，不把非局部Throw与ordinaryfalse混淆。本次随FsEntry提交，不重复先前common批次。
