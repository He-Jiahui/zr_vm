---
related_code:
  - zr_vm_library/include/zr_vm_library/file.h
  - zr_vm_library/src/zr_vm_library/file.c
  - zr_vm_library/src/zr_vm_library/file_path.c
  - zr_vm_library/src/zr_vm_library/file_path_internal.h
  - zr_vm_library/src/zr_vm_library/file_copy.c
implementation_files:
  - zr_vm_library/src/zr_vm_library/file.c
  - zr_vm_library/src/zr_vm_library/file_path.c
  - zr_vm_library/src/zr_vm_library/file_copy.c
plan_sources:
  - .codex/plans/20261016-review-bugfix.md
tests:
  - tests/library/test_file_path_operations.c
  - tests/cmake/file-path-operations-tests.cmake
  - tests/library/test_file_list.c
doc_type: module-detail
---

# 文件路径、复制和移动的失败边界

## 模块职责

原来的 `file.c` 同时承担路径词法处理、宿主流、目录列表和递归复制，超过 1800 行。本次把路径函数移到 `file_path.c`，复制与移动移到 `file_copy.c`；`file.c` 保留宿主文件、流、目录和删除操作。`file_path_internal.h` 只声明这些翻译单元共同使用的私有路径 helper。现有 CMake 模块源文件收集规则直接纳入新增实现，没有保留另一份旧实现。

## 路径输出

所有内部 join 调用检查 `snprintf` 的有符号返回值与实际输出容量。编码错误或截断时清空输出，分别设置 `EINVAL` 或 `ENAMETOOLONG`，返回 false。公共 `PathJoin` 保持 void 签名，失败以空输出和 errno 表示；绝对路径构造使用能返回失败的内部 join。

Windows UNC 路径保留起始的两个分隔符，之后的重复分隔符仍压缩；词法归约把 server/share 作为根，避免 `..` 越过该根。`E:relative` 属于盘符相对路径，`IsAbsolutePath` 返回 false，当前 normalize 明确拒绝该输入，不把它改写为 `E:\relative`。此处不访问 UNC 网络主机。

## 修改目标前的检查

Copy 和 Move 先规范化两条路径并检查源确实存在，再处理同路径与组件祖先关系，之后才允许删除覆盖目标。

- 已存在且规范化文本相同的源和目标返回成功，不执行删除；overwrite 为 true 或 false 都如此。
- 目标是源的严格组件祖先时拒绝，保护源及祖先目录里的其他数据。
- 目录源的目标位于其内部时也拒绝，避免递归复制不断扩展源树。
- 只比较完整路径组件，`source-other` 不会被当作 `source` 的子目录。
- 源不存在时返回 false 和 `ENOENT`，保留已经存在的目标。

这些规则是规范化后的文本与组件关系检查。它们不识别硬链接、符号链接、大小写别名或其他物理同物路径，也不提供并发文件系统事务、源可读性或复制原子性保证。检查存在性后仍可能在打开文件或枚举目录时失败；已有 close 错误处理不在本次修复内。

## 实际回归

`zr_vm_file_path_operations_test` 调用实际 Library 文件 API，Windows 上注册 13 个独立进程：同路径 Move 两项、祖先目标 Copy/Move 两项、缺源 Move、join 截断、UNC、盘符相对路径，以及同路径 Copy、普通 Copy/Move、组件前缀兄弟目录和普通 join 五项对照。

稳定 `meta-thread-file-red-r2` 在 configure/build 成功、1918 个输入哈希无变化的条件下复现了八项失败，五项对照通过。修复后的 `support-batch-5-green` 实际重新构建成功，13 项及已有 `file_list` 均通过；整批 143 项 CTest、pool 14 项和 CLI 示例通过，1929 个输入哈希无变化。

目录后代拒绝与祖先拒绝共用路径关系 helper，已独立静态复核；当前没有执行可能无限递归的旧实现 RED 用例，不给该分支单独的动态回归信用。已有 `file_list` 测试同时检查模块拆分后的列表、路径与目录调用链。

所有测试根在运行前登记于任务 artifacts 清单；夹具拒绝复用已经存在的根。进程结束后只按明确路径和文件哈希清理自有数据，保留 RED/GREEN 证据，不扫描删除未知目录。
