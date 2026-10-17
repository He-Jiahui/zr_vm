---
doc_type: code-review-batch
related_code:
  - scripts/syntax_migration_embedded.py
  - tests/scripts/test_syntax_migration_embedded_positions.py
  - tests/cmake/syntax-embedded-positions-tests.cmake
implementation_files:
  - scripts/syntax_migration_embedded.py
plan_sources:
  - docs/code-review/comment-standard.md
  - docs/testing-and-validation/syntax-inventory-host-positions.md
tests:
  - tests/scripts/test_syntax_migration_embedded_positions.py
  - tests/cmake/syntax-embedded-positions-tests.cmake
---

# 嵌入字符串的宿主坐标契约

本批逐项审查3份首方文件的13个单元：2个类型、8个函数及3个入口/配置块。为每项补充中文纯#文档，共13段、14个@param和3个@return标签；其余无返回值的测试或入口以正文说明None、SystemExit或配置结果。当前493份台账交叉核对后，将旧生成器身份更新1项并新增12项，全部13项为commented，无未审项或新BUG/TODO。

三个文件原始程序前缀、既有docstrings及人工字符串逐字保留。只在EOF追加文档，不改公开签名、程序行为、CTest条件或测试，也不创建源码副本。独审提出的两处文字已收紧：bootstrap的字节码缓存说明限定于现有-B启动；main守卫说明采用长期入口语义。原提案与修订、独审和逆向还原证据分别保留。

## 职责、调用与生命周期

EmbeddedStringPayload保存decoded文本和宿主字符边界的冻结快照，source_offsets含尾后哨兵，line_offsets记录解码换行边界。坐标属于同一Python str的字符域，不能直接用作文件字节或LSP UTF-16偏移。自动dataclass构造器不验证关联不变量；source_offset要求有效的一基行列，不提供参数拒绝协议。

生成器只实现有限引号、注释和转义规则，逐片段yield记录；它不求值宿主表达式、不拼接C相邻字面量，也不执行JavaScript插值。暂停时持有输入与局部状态，已发布的冻结记录可独立持有；无文件句柄或显式Free协议。Python分配异常向迭代调用方传播。_scan_path经import alias接入，分别映射finding起点和尾后终点，上下文过滤仍由该调用方负责。

四个unittest方法使用原有内存字符串。支持scan仅在with中替换Path.read_text，正常或异常离开均恢复；该类方法替换在进程内可见，不提供并行隔离。断言器按首个拼写和同一行范围构造预期，不能当作通用多行校验器。unittest发现和__main__是间接入口，CMake在Python变量为真时注册现有脚本，变量非空不证明解释器可执行。

## 历史身份与问题复核

旧scripts_remaining.tsv生成器键为syntax_migration_inventory._iter_embedded_string_payloads。已从e105be原Git blob精确恢复对应旧记录与函数正文；b0f258a提交引入边界映射与现有回归测试，当前import alias保留调用关系。这是提取与合同变化，不是字节不变的搬移。本批将唯一旧行原位更新为当前文件/生成器，另外追加12行；不另建重复台账或对旧身份新增授信。

旧“解码换行直接成为宿主第二行”的BUG理由已不描述当前实现：source_offsets把两个边界投回宿主，再由_position_at计算行列。历史复现保留在旧提交，本批只更新当前审查记录，不把此前修复算作本任务行为修改。8个完整依赖、826个保留EOL的行pin和完整函数/类型正文支持当前契约；其他扫描器定义只作依赖，不重复授信。

## 修改前后验证

实际BEFORE和AFTER均用真实E盘Python执行同一现有脚本，四个case身份与结果全部相同，child均exit0，完整stdout/stderr逐字相同。六个有限源码输入在各次运行首尾稳定，跨pair只含本批三份EOF注释。原BEFORE驱动因^OK$未兼容CRLF而actual exit1，原results/日志未改；Root独立按真实换行复核四项ok及Ran4/OK，AFTER驱动按splitlines判读并actual exit0，不把原驱动失败改记为成功。

严格等价门核对891个Python原程序token：kind、spelling及实体坐标一致，AST含全部原docstrings与坐标、同文件名compile-only marshal和递归字节码签名完全一致。5个EOF虚拟DEDENT/ENDMARKER坐标在原最后一行之后后移，单独披露，不宣称全部虚拟坐标不变；CMake原程序前缀及EOL保留，新增内容全为#注释。未运行模块CLI或创建物理pyc。

当前构建缓存中的Python指向WindowsApps别名，本批未调用该别名、未重配共享CMake。直接测试使用实际E盘Python3.12.10和-B，TMP/TEMP指向登记的E盘任务根；不生成宿主fixture或新测试。CTest注册内容保持原样，直接Python执行不冒充CTest、C构建、真实宿主求值或所有扫描规则验证。

全部审查行纳入既有scripts_remaining.tsv，只有旧生成器行及12个新增键改变；其它行保持原字节。inventory仅更新所审3行的sha256/status/note，其余列及外部行保留。实际源码/台账独审正在进行；暂存差异逐项核对、git diff --cached --check及串行提交尚待后续各自门禁。必要E盘日志与复用产物保留，释放0B，产物生命周期仍开放。
