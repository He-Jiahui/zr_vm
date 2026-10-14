---
related_code:
  - zr_vm_lib_system/include/zr_vm_lib_system/fs.h
  - zr_vm_lib_system/include/zr_vm_lib_system/fs_registry.h
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_core/src/zr_vm_core/string/string.c
  - zr_vm_core/include/zr_vm_core/string.h
  - zr_vm_parser/src/zr_vm_parser/syntax/lexer.c
  - zr_vm_lib_system/CMakeLists.txt
  - zr_vm_common/CommonMacros.cmake
  - zr_vm_common/include/zr_vm_common/zr_array_conf.h
  - tests/system/test_system_fs_module.c
  - tests/module/test_module_system.c
  - tests/cmake/compiler-project-targets.cmake
implementation_files:
  - zr_vm_lib_system/include/zr_vm_lib_system/fs.h
  - zr_vm_lib_system/include/zr_vm_lib_system/fs_registry.h
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_registry.c
plan_sources:
  - docs/code-review/comment-standard.md
  - user: 2026-10-14 System.Fs public callback and registry contract review
tests:
  - tests/system/test_system_fs_module.c
  - tests/module/test_module_system.c
  - tests/cmake/compiler-project-targets.cmake
doc_type: workflow-detail
---

# System.Fs 公共接口与注册表契约审查

## 范围和逐单元清点

本批覆盖 `fs.h`、`fs_registry.h` 和 `fs_registry.c`，逐单元表 `docs/code-review/coverage/zr_vm_lib_system_fs_public.tsv` 共 193 个唯一 `(file, unit, kind)`：`fs.h` 44 项（文件契约、guard、42 个 callback），`fs_registry.h` 3 项（文件契约、guard、getter），`fs_registry.c` 146 项（文件契约、条件宏、数组/descriptor 定义及逐项 initializer）。决策为 62 `commented`、117 `no-comment`、11 `TODO`、3 `BUG`。其中 16 个参数元素各有独立 no-comment 行。其中 registry 的逐项记录覆盖实际声明的命名字段、方法、类型、函数、接口、meta 和 hints；不把测试数量当作语义审查完成数。

注册表的 116 个静态 initializer 已逐项核对：12 个参数数组含 16 个参数元素，16 项分别登记为独立 initializer 行；另有 98 个具名条目——7 个 `kTypes`、22 个字段、34 个方法、2 个 interface entry、2 个 meta entry、12 个兼容函数和 19 个 hints——再加 1 个 JSON schema 与 1 个 `kModule` descriptor。两个 meta entry 分别是 `FileSystemEntry` 构造器和 `FileStream` 自动 close hook；两者 arity 均为 1..1，只计显式参数、不含 receiver：构造器参数是 path，自动 close 参数是 VM 额外提供的关闭状态值（正常 null）；close callback 不读取该状态。命名 FileStream.close 的显式 arity 为 0..0。leaf 中这 116 个静态 initializer 均各自对应唯一记录；12 个参数数组定义另有独立单元行。这里的数字是代码 initializer 清点，不是功能测试覆盖数。

最初修改前 SHA-256 为：`fs.h` `332de7c9acda490d7a6c1da86bfe37d2dc74c36fe89d42c6dd3c0dd6aa326b05`、`fs_registry.h` `7127bbff1b3283b0e8c18ba7d4d361b3a00a677f5825a457091c0b6a3556a65f`、`fs_registry.c` `77708fd280dcf6e6357482a31830a10a59387438900354d78b499c8d890645c0`。D6 是较早的测试 checkpoint；其 SHA-256 为 `fs.h` `d6e3588a2cda45e36ab949d7913930091f549becf96e3c27009834475795d1c5`，其后的 E228 checkpoint 为 `fs.h` `e228b0cbe6a8288a430465b0d02a6655db94cd558c4a7f70e81839bcdf332d6a`，两者的 registry 文件均为 `fs_registry.h` `b4d7b4e0c798d43acc69a5f38861264fe10ef4a29b3c4261881cb36df3e4568c`、`fs_registry.c` `a8c9e3ca10d1d398c4d022af5641f3808a4f92e856e3acc4d902c72fc701c6f6`。当前冻结 SHA-256 为：`fs.h` `4dec12df9c735dd8067273d4248171ab6aa16165bebd33c6002665601b6204a8`，fs_registry.h 仍为上述 B4D7，fs_registry.c 最终为 50854ac2a36fa763e58e2b7896d3dcd1c44c595ae7a897fed67112f2a59dd71c（第 83 行纠正 meta 参数计数说明，未改 descriptor）。50A 是 Root 修正 fs.h:304 之前的验证 checkpoint；其 proof/build/suite 证据只属于当时字节。4DEC 与最终 5085 的 proof/build 见下方 Root r7 记录；50A 的 suite 只属于历史 checkpoint。

## 注册、参数和资源契约

`zr_vm_lib_system/CMakeLists.txt` 经 `CommonMacros.cmake` 将 `src/*.c` 加入模块目标；`module.c:80-100` 将 `ZrSystem_FsRegistry_GetModule` 返回的静态描述符放入系统叶模块列表。getter 返回只读借用指针，调用方不得释放或修改；注册器浅拷贝描述符不会延长 provider image 中嵌套数组、字符串和 callback 的有效期。`kTypes`、`kFunctions`、`kHints` 和 `kHintsJson` 的 consumers 是 `kModule` 初始化字段（分别为 `fs_registry.c:364-365`、`:362-363`、`:366-367`、`:368`），外层聚合通过 `module.c:83` 取得该模块。

产品 include 链已先由 `zr_vm_common/include/zr_vm_common/zr_array_conf.h:13` 定义共享 `ZR_ARRAY_COUNT`；`fs_registry.c` 中的 `#ifndef` 本地 fallback 在当前产品配置下不启用。因此该备用宏有审查记录，但没有把它误记为当前构建的运行 consumer。

dispatcher 在 callback 前完成 descriptor 元数检查，并把脚本结果槽预置为 null；callback 的 C `ZR_TRUE`/`ZR_FALSE` 表示本地 callback 完成状态，不总等于脚本 bool。必需字符串类型错误经 `ReadString` 抛 VM `TypeError`；optional bool 类型不符由 `ReadOptionalBoolArgument` 返回普通失败；optional int 通过 `ReadInt` 的异常路径；optional string 缓冲区不足可普通失败。各 callback 的宿主 I/O 失败由具体实现决定是脚本 false、普通 null 还是 `IOException`，逐项结果写在头文件和 leaf 表中。

参数缺省由 callback 而非 parameter descriptor 实施。`glob` descriptor 接受 1..2 个参数；`recursively` 的默认 true 在 callback 中处理，现有 `glob` 用例显式传 true。File/Folder 创建用例也传入 true，Folder 删除的默认 false、read count 默认 -1、seek origin 默认 begin，以及其他 optional 参数的默认路径没有专项默认值测试。hints 和 metadata 测试核对导出形状，不证明这些 callback 或提示 JSON 的行为。

路径包装对象保存规范化路径和查询时点 metadata 快照；`FileStream` 的宿主句柄由私有 `ZrSystemFsStreamData` 持有，`__zr_ffi_handleId` 是 FFI lowering 使用的整数包装，不拥有句柄。只有宿主 close 成功后才将私有 `closed` 标记设为 true；失败时 `closed` 仍为 false，但这不证明宿主句柄仍开放或必可重试，实际状态依平台语义待核。File/Folder I/O 完成后仅尝试刷新快照，刷新失败不回滚已完成 I/O。string/metadata void setter 有软失败边界，callback 返回 true 不保证 setter 一定写入。

## 已证实缺陷和待核风险

三条 `BUG` 行对应三个 C callback 单元中的同一 NUL 截断缺陷；它们经四个 descriptor 暴露：模块级 `readText`、`File.readText`、`IStreamReader.readText` 和 `FileStream.readText`。`ReadTextFromHandle` 按字节数读取后添加 NUL，再经 `ZrLib_Value_SetString`、`native_binding_create_string` 和 `ZrCore_String_CreateTryHitCache` 复制；`ZrCore_NativeString_Length` 在 `string.h:131` 使用 C 字符串长度，所以合法文件/流内容 `A\0B` 返回时只保留 `A`。代码路径锚在 `fs_stream.c:313-352`、`fs_entry.c:47`，并由三个 wrapper 调用。合法脚本字符串可由 lexer 的 `\x00` 路径保留显式长度（`lexer.c:51-78,262-275,302`）；byte I/O 可携带含 NUL 的数据，因此该现象不依赖无效 VM 字符串。现有测试没有含 NUL 的读回断言；路径/复制用例只读普通 ASCII，stream 行为用例在读取前失败。

11 条 `TODO` 行限定在尚未闭合的借用、GC 或平台失败状态：构造 callback 的 path 跨 `ResolveConstructTarget`/`PopulateEntryObject`；`File.open` 的 `self.fullPath` 跨 `NewStreamObject`，且隐藏 owner setter 软失败可能发布字段不完整的包装对象；File 与 Folder 的 `copyTo`/`moveTo` 显式 `targetPath` 跨后续分配；Folder `entries`、`files`、`folders`、`glob` 的 C 局部结果数组跨对象物化；`FileStream.close` 失败后宿主 handle 的实际状态与重试语义。需继续核对 native fast/inline lane 的参数保持、有效 GC root、可达 collection 时机和平台 close 语义；本批没有 GC/fault injection，未将这些疑点标成 BUG。`Stream.readText` 对 count 截断多字节 UTF-8 的处理在该 callback 行的约束中单独注明，未另增重复 unit。

117 条 `no-comment` 记录中，16 条是逐项参数 initializer 记录；三个预处理项是两个 include guard 和条件 `ZR_ARRAY_COUNT` fallback：guard 只负责防止重复包含；本地 fallback 因共享宏先定义而不参与当前目标的宏展开。其余条目主要是静态 descriptor literal，已直接列出字段、方法、类型、接口、参数、callback 或 hint 的名称与形状；相关运行约束已由对应 callback 文档、表级契约或聚合描述覆盖，因此不需要逐项重复源码注释。各项实际测试边界仍以 TSV 的逐行记录为准。

## 现有测试的实际边界

`tests/system/test_system_fs_module.c` 的 `zr_vm_system_fs_test` 注册 8 项：metadata/export 检查、路径对象与目录操作、copy/readText、tellFd 元数据探针、stream modes/using、缺失文件 IOException、closed stream lowering 和普通调用 lowering。`tests/module/test_module_system.c` 中的共享 leaf metadata 断言只确认导出和静态类型；不把它当作 callback 行为测试，也不重复登记其 canonical test unit。

Root 的修改前基线构建 exit 0，FS 用例为 7 PASS / 1 FAIL。失败发生在 `test_system_fs_source_runtime_supports_stream_modes_and_using`：脚本 `takeReader(stream)`（`:976`，测试框架报告在 `:1035`）触发 `KNOWN_NATIVE_MEMBER_CALL_RECV_U8 invalid cached native member callable`。这是 callback 调用之前的 lower native-member/cache 路径，不能据此归为 FS callback 缺陷。该处之前的写入、定位和状态断言已到达；后面的 `readText`（`:978`、`:987`）、`setLength`（`:979`）、追加流、`writeBytes`/`readBytes`、关闭与 using 检查没有执行。tellFd probe 的模板（`:1324`）仅作编译/元数据探针，不执行其流操作。测试没有调用 `FileStream.flush`；路径对象用例没有覆盖 `appendText`；默认参数、NUL、UTF-8 边界、故障注入和 GC 窗口也没有专项断言。

历史 D6 checkpoint 的 Debug build exit 0；module suite 70/78 PASS、provider suite 9/9 PASS、FS suite 7/8 PASS。D6 的 FS `takeReader` 失败和逐例状态与修改前基线及联合 native r1 一致；构建/测试期间 2975 个输入 pins 稳定。D6 严格注释等价 proof SHA-256 为 `0cbf4af205e71c71d49667c04b299186fa1cd0de86af855da8bfd572213ce06b`；native results SHA 为 `8769fcf2a5c7d6aa1df26320d74958599f0be9208100fd3ec8a85f9c3def8e37`，逐例比较 SHA 为 `5a780173478a944ccedad21169541dde6cb7abaaa8e1e87600097934589e1b6b`。后续 build-only checkpoint 使用 6BD 版头文件，构建 exit 0；results SHA `cfe0b0b6bd235a6cacf525bc6a87a415a1f8b179c3e7e0614472e0da4a18abf2`，comparison SHA `f5dbd0525c38125a34282b1742ec2d6871e0cf864f7000e728592d552e33ab87`，没有运行 suite。历史 50A 源码的严格注释等价 proof SHA-256 为 `610b291e67008f5b1a5e0be152867ae290f3469351e9f324298532b57cd1476c`；validation linkage SHA 为 `410f7e02416860de6f3e391ed9642423152da0eb87a975ba13a3dd633d2e0911`。该历史 proof 确认当时三源注释变更不改非注释 token/行映射，但没有重跑 build 或测试；旧 D6/6BD 执行证据不能声称为当前 50A 的执行结果。相关记录位于 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/`。本批没有新增测试、行为修复或手工夹具；Root 登记的自动生成 fixture 共 12 files / 39 bytes，按既有 manifest 保留。

## Root 最终验证与证据归属

后续 complex-final-native-r1 在当时三源（50A/B4D7/A8C9）与 Complex 四源冻结后实际执行：Clang 19 Debug 构建 exit 0，module suite 70/78 PASS、provider suite 9/9 PASS。module 的八项失败及全部逐例结果字符串均与保留的 D6 native 基线一致；运行前后 2975 个输入 pins、七个冻结源文件和 CMake cache 均稳定。results SHA-256 为 c689e859132e1dab97442046e39ced7b4a83cf2d1868442ec57a76c2d3ecbe67，comparison 为 2d654884b6bbe3466c271d66581965a348effc4738e2f99131605af14b961a50，dependency audit 为 285407f034937304b1b3bc9694b63e084d1c009fb3d1a39254106ddd8334abb7。构建包含 System.Fs 五个对象、系统静态库及相关可执行文件；也重编译了其他会话的 compiler_quickening.c 和 compiler_typed_metadata.c，因此这是当前共享 checkout 的验证，不能把所有执行行为单独归因于本批注释。

这次没有重跑当前 50A 的 FS suite；FS 7/8 的运行证据仍限定在上述 D6 checkpoint。构建保留已有 initializer/未使用变量等警告，未作行为修复，也未新增 NUL、UTF-8、默认参数、AppendText、Flush、故障注入或 GC 测试信用。构建及两个 suite 子进程均已自然退出，临时目录前后无新增文件；本批释放空间为 0 bytes，12 个 FS 生成文件共 39 bytes、复用构建、共享缓存和失败证据按产物台账保留。

历史 177 行 checkpoint 的 leaf SHA-256 为 `446107e0234eedbb210ba48f84c4f0a00822f1a5b5593d1da341c0fea8ee1fb4`；Root 当时逐行核对 177 个行哈希并独立运行 check-batch，自然退出 0，结果 SHA-256 为 `d507d725d21d80d4198e98f8bbc8af9652588b988a32e9bcb3e0625588708f27`。该结果只校验当时 177 行；193 行最终 leaf SHA-256 为 fd434a790a59cb65fe4f1305d3101cf5c5c15155d6dd2d98b1ae7b2b463a54d2；Root 逐行复核原 177 行和新增 16 行，并精确更正三处参数定义锚点及两个 close-meta 单元，独立运行 check-batch 自然退出 0，结果 SHA-256 为 c228c988fe398306756afb3e5d9dacae292f923811f153e9dfbc27777656fc99。结构校验只证明九列、唯一键及锚点边界，契约真实性仍由调用路径复核和独立 reviewer 另行核验。


当前最终 r7：三源码 SHA 为 4DEC/B4D7/5085，严格注释等价 proof 4ecdd8f159a9c876a5d9a94b7c54ee34539726e8ce55b3a43e36a722fc9bff4d，共 2868 项非注释 token 与空白/行首属性等价；完整行映射与 r6 一致，linkage f359a069256d0c1aa68c7d7f858a948df0471cd0f32e192659d65798fb18343f。复用 Clang 19 Debug 构建自然退出 0（14.500 秒），results ee7d936213ec77c670e66f6af44fcca2c5a09751e5aa65a682c733222ad15d40，comparison 73d8a666960d1920b5dcab30001b92f1e02314f7057067724b15798275766601；2975 输入、七个冻结源和 cache 均稳定，保留原有 initializer 警告。r7 只构建并链接三个目标，没有重跑 suite；module/provider 的实际执行仍限定于 50A，FS suite 仍限定于 D6。r6 外层断言因基线到启动间 W2 测试变动退出 1，但实际构建退出 0；dependency audit 64cef61582e1a601a04cbd775d491bc4f1dc6d67f015845f7f36de6c4ca0a448 已证实该 include 及实际引用它的 C 文件不属于三个构建目标，r7 新基线未发生这项漂移。所有自有子进程已退出；本批释放 0 bytes，保留项和既有清理拒绝状态不变。
