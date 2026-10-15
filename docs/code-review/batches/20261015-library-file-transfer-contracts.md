---
related_code:
  - zr_vm_library/src/zr_vm_library/file.c
  - zr_vm_library/include/zr_vm_library/file.h
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_registry.c
  - tests/system/test_system_fs_module.c
  - tests/library/test_file_list.c
  - tests/cmake/zr_vm_file_list_tests.cmake
  - tests/cmake/compiler-project-targets.cmake
implementation_files:
  - zr_vm_library/src/zr_vm_library/file.c
  - zr_vm_library/include/zr_vm_library/file.h
plan_sources:
  - docs/code-review/comment-standard.md
  - user: 2026-10-15 File Copy/Move host transfer contract review
tests:
  - tests/system/test_system_fs_module.c
  - tests/library/test_file_list.c
  - tests/cmake/zr_vm_file_list_tests.cmake
  - tests/cmake/compiler-project-targets.cmake
doc_type: code-review-batch
---

# Host File 复制与移动契约

## 范围与结论

本批只补齐六个已登记单元的中文 Doxygen 契约：`file.c` 中两个复制 helper 与 `ZrLibrary_File_Copy`、`ZrLibrary_File_Move` 定义，以及 `file.h` 中 Copy、Move 两个公开声明。未新增台账键，未修改实现、签名或测试。六个单元均标记为 `BUG`，对应三个可由本仓调用闭合的缺陷家族（祖先覆盖删源、Move 在源校验前删目标、目录后代自递归扩展），不表示六种独立故障。

| 文件 | 单元 | kind | decision | 已证触发条件 |
| --- | --- | --- | --- | --- |
| `zr_vm_library/src/zr_vm_library/file.c` | `file_copy_file_contents` | function | BUG | overwrite=true、目标为源严格词法组件祖先、Delete成功且无外部重建：先删源后OpenRead失败。 |
| `zr_vm_library/src/zr_vm_library/file.c` | `file_copy_directory_recursive` | function | BUG | 同一祖先覆盖条件下先删源后List失败；实际目录复制到自身严格后代还会把新目标继续作为下一层源递归。 |
| `zr_vm_library/src/zr_vm_library/file.c` | `ZrLibrary_File_Copy` | function | BUG | 传播祖先删源与目录后代自递归扩展；相等规范化字符串则两种overwrite均成功空操作。 |
| `zr_vm_library/src/zr_vm_library/file.c` | `ZrLibrary_File_Move` | function | BUG | 覆盖目标在源校验/rename前删除；缺失源、相同规范化路径或源位于目标祖先下时可能返回假且数据已丢。 |
| `zr_vm_library/include/zr_vm_library/file.h` | `ZrLibrary_File_Copy` | function | BUG | 公开契约说明覆盖目标祖先目录会删除包含源的目标树。 |
| `zr_vm_library/include/zr_vm_library/file.h` | `ZrLibrary_File_Move` | function | BUG | 公开契约说明预删除目标后 Move 失败的可观察数据损失。 |

## 调用链与分派

`ZrLibrary_File_Copy` 规范化两条路径并检查源存在性，然后按源的实际 `stat` 类型进入目录 helper 或文件 helper（`file.c:1214-1239`）；文件 helper 也由目录递归处理子项时调用（`:866-872`）。目录 helper 先创建目标，再取得源的直接子项列表，并根据各项类型递归（`:851-875`）。System.Fs 的 File/Folder `copyTo` 分别在 `fs_entry.c:529`、`:663` 调用 Copy，并由 `fs_registry.c:157-162`、`:180-185` 绑定。包装对象的名义类型不会限制底层库对源 `stat` 结果的分派。

`ZrLibrary_File_Move` 的 File/Folder `moveTo` 调用点分别是 `fs_entry.c:552`、`:682`，绑定位置是 `fs_registry.c:159-162`、`:182-185`。Move 先按 `overwrite` 处理目标，再尝试 `rename`；任意 `rename` 失败都会进入 Copy 后 Delete 源的回退（`file.c:1255-1269`）。此顺序不能被描述为仅针对跨卷回退，也不提供事务性或原子性。

## 已证实的失败副作用

Copy 的文件和目录 helper 在 overwrite=true 时都会在读取/枚举源之前尝试递归删除已存在目标（`file.c:772-775,847-853`）。若 overwrite=true、目标是源的严格词法组件祖先、该 Delete 成功且无外部重建，该删除会一并移除源及目标树中的其他内容；之后打开或枚举源失败，返回假却无法恢复已删数据。此为两个 helper 与公共 Copy 定义/声明记录的同一缺陷类别。

Move 也在尝试 `rename` 或验证源可复制之前删除已存在目标（`file.c:1255-1269`）。限定 overwrite=true、现存目标递归删除成功且无外部重建：同一规范化路径会删除源本身；源缺失时先删除目标再因后续失败；目标为源严格词法组件祖先时还会删源和兄弟项。这些合法输入可能使 Move 返回假但已有数据丢失，故在实现和公开声明中明确标为 `BUG:`。

BUG: 实际目录源的严格词法后代目标缺失（或允许覆盖），且建目录/枚举成功、无外部路径变化时，先建目标再枚举源（file.c:851-875）会将新目标纳入源快照，再以该新目标为下一层源重复复制。实现没有目标排除、环/深度拒绝，可留下嵌套部分树；file_join_into:658-666 只判断 snprintf>=0，不能以固定缓冲推断稳定错误或可靠终止。Move 仅在 rename 失败进入 Copy(true) 回退时到达此路径。规范化字符串相等的 Copy 空操作也只比较规范化后的字符串，不等同于对硬链接、符号链接或平台别名做物理身份判断（`file.c:1233-1235`）。

文件复制按 64 KiB 块读写；读取、短写或目标打开失败会返回假，但在此之前的目标删除、父目录创建和已写字节不会回滚。成功状态在两个句柄关闭前设定，CloseHandle 的结果被忽略（`file.c:809-832`）。目录复制在首个失败子项处停止并释放列表，既有子项副作用保留（`:856-879`）。Move 回退中的 Copy(true) 和源 Delete 是独立操作：Copy true 后仍可删源，即使其文件 close 曾报错；Copy false 不执行最后的源 Delete，但此前目标预删除可能已移除源。两边均可能留部分状态。close 的忽略仅记录为返回契约，不在没有更强预期证据时另立 BUG。

## 现有测试证据及边界

`test_system_fs_source_runtime_supports_path_objects_and_directory_operations` 在 `tests/system/test_system_fs_module.c:757-760` 对新目标执行文件 Copy/Move，并断言复制内容、移动目标存在和原复制路径消失；`:776-779` 对新目标目录执行 Copy/Move 并断言目标子文件存在。`test_system_fs_copy_result_supports_exists_and_read_text_separately` 在 `:881-887` 对另一个新目标复制并断言存在及文本相等；两函数由该测试文件 `main:1523-1524` 注册，目标定义见 `tests/cmake/compiler-project-targets.cmake:186-189`。它们没有覆盖 overwrite 对已有目标的替换、同路径、祖先/后代关系、失败部分副作用或 rename 回退分支。

`tests/library/test_file_list.c:74-123` 的五个用例只测空目录、glob、列表内容与排序；文件中没有 Copy/Move 调用。它通过 `tests/cmake/zr_vm_file_list_tests.cmake:3-10` 注册 `file_list` target，CMake 配置见 `tests/cmake/compiler-project-targets.cmake:184`。该 target 与 Copy/Move 的行为覆盖无关，不能用它作为本批成功证据。

producer 未运行构建或测试；Root 修改后实际验证见末段。lease 登记的修改前基线（results SHA-256 `de826a391eccc12ca129ff63a853b4d7ceddafa84cb913554edcf126c68a4384`）由 Root 保存：系统 Fs 组 8 项中 7 项通过，已知失败为 `stream_modes` 在 `takeReader` 的既有 `KNOWN_NATIVE_MEMBER_CALL_RECV_U8` 错误；Copy/Move 的普通新目标用例通过。该基线只支持上述具体成功路径，不覆盖目标替换、同路径、祖先/后代关系、失败副作用或 rename 回退。`file_list` 中没有 Copy/Move 调用，也不以其列表测试结果作本批行为信用。


## Root 收口与验证身份

所有关系按 NormalizePath 实际输出的字符串/组件解释，假定相应源/目标状态与静态路径链一致且没有外部重建、改名或 cwd 变化，不扩张为 symlink/hardlink/reparse/mount 物理别名检测或并发原子性保证。Copy 源存在后才判断字符串相等；该显式空操作对两种 overwrite 都成立，overwrite=false 的现存目标拒绝只适用于不同字符串路径。Move 没有这一空操作分支。

R2 历史源 SHA-256：C `15b23ed0c41779146ed4e8ae379ab426ed572d33ba893231dd2914b756cae305`，H `b32e75d5781422cfe5b15bb0866b32f08061f297585d270dc283b4a2a37eb264`。完整严格证明 `library-file-transfer-comment-equivalence-r2.json` SHA-256 `2b4c18d288fc6ad67069558b233c9ba4ea6d96607af7b77ce04879482979f359` 已通过，全部非注释 token、StartOfLine/LeadingSpace 与原始程序物理行位置一致；四个 lexer 输出已完整消费且等待退出，无源码副本。六键审计 `library-file-transfer-six-key-audit-r2.json` SHA-256 `ff12e4ee0b3354b2e9dbf4807efebe864343bfb71f66d6dddd77ddc34baff76a` 保留其余227行原字节，六个旧 no-comment 更正为 BUG，不增加累计单元。Root after native 结果见末段。


Root R3 只更正目录 helper 的一个 EOF 术语：递归分类取列表 children.entries[].existence（file.c:865），不描述成该 helper 直接 stat；公开 Copy 的初始源分类仍由 file_stat_exist（:1228）完成。当前最终 C SHA-256 `ac43d93b6538996ed204564a9c03285c07623a8150cccb3991939826661854ee`、H `b32e75d5781422cfe5b15bb0866b32f08061f297585d270dc283b4a2a37eb264`。最终证明 `library-file-transfer-comment-equivalence-r3.json` SHA-256 `073fdcbc3b054ade72aaa6aeea9243a4c8271aa69d5bd78f2bec4edbb780aeb4` 通过9427个非注释token/四个已等待lexer，flags和原程序物理行位置一致；上段R2身份保留为历史修订证据，after native 结果见末段。


完整调用/资源审计 library-file-transfer-caller-resource-audit-r3.json SHA-256 e43580a1ddc261f8708ff3ce7dfe29d04c391f850ae97beda691cb1e91b6b4ad 将程序中的17个出现项全部分类为4定义、4声明（含 C 内部前置声明）和9直接调用；另沿 System.Fs descriptor 注册追踪跨语言入口。它核对11个源/构建/测试输入及六个旧键，普通测试仅给初始不存在目标的 Copy/Move 路径信用。该报告的 after_native_pending=true 是当时的审计状态；实际修改后结果如下，不回写历史收据。


## Root 修改后实际验证

在实际 checkout 复用 clang-cl Debug 构建，未运行 configure、未复制源码或新增测试。统一构建 exit 0（100.640 秒）；GC scopes 默认进程 10/10、multimutator 11/11、known-native 61/61，三个进程均 exit 0；Fs 进程仍 exit 1、7/8，失败仍是 stream_modes 的既有 takeReader/KNOWN_NATIVE_MEMBER_CALL_RECV_U8 路径。与各自修改前实际运行逐条比较，所有 Unity case 行及退出码一致。Fs 的普通 Copy/Move 新目标用例通过，不增加对同路径、祖先/后代、覆盖失败、rename 回退、输出别名/GC rooting 或未执行分支的运行信用。

实际报告位于 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463：core-gc-object-host-final-native-r1-results.json SHA-256 679f0f5cfd4173c330b0d700f6f64d93674cd522b46bc454428b94a44a0d8655；comparison.json（同前缀）SHA-256 3ca44f91f23d306973d8d34f26307963ac96ec75e4d5956f2f10c664279a1e8e；summary.json（同前缀）SHA-256 c9c3384d2449ac8fd357d7929526dfaefb3600ce8c41e6da7c7b65e838120a6c。3216 个保守输入及 45 个冻结文件在构建/测试前后保持哈希一致，全部子进程均已等待退出。历史实际输入与当前源码之间的六个 raw 差异由四份完整严格证明桥接，共 18298 非注释 token / 12 个已等待 lexer，flags 与原程序物理坐标相同；这是原程序等价与现有执行结果证据，不称为原始源码字节完全相同的隔离实验。

本轮释放 0 B。复用构建、缓存、必要日志和 40 个 Fs 运行夹具文件（130 B，本次新增4个/13 B）保留并登记；先前自动审批已在创建清理进程前以 blocked by policy 拒绝清理，因此没有再次尝试，清理状态仍未关闭。
