---
related_code:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_union.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c
  - zr_vm_core/src/zr_vm_core/value/value.c
implementation_files:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_union.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h
tests:
  - tests/debug/test_debug_variable_child_shape.c
  - tests/cmake/debug-runtime-targets.cmake
  - tests/cmake/debug-metadata-aot-registration.cmake
plan_sources:
  - user: 全量首方审查、按需补充中文职责和生命周期注释
doc_type: module-detail
---

# Union 调试检查合同

本批逐项复核56个既有单元：debug_union.c中35函数、3宏、1类型和4重要块共43项；debug_internal.h中ZrDebugUnionView和12接口共13项。52段中文Doxygen覆盖职责、使用时机、169个参数方向、适用返回结果、不变量、资源和暂停时序及References；重要块随父合同。结论51commented、2TODO及3BUG镜像，选中项均有结论；两组标签分别仍是一个既有布局TODO家族和一个既有显式GC根滞留BUG家族，不新增问题。43个C旧键从扁平目录迁至inspect，13个H键保持；不新增单位、接口签名或程序行为。

Union视图把boxed值和frame内联存储统一为展示所需的case、payload及类型信息。展示形状不建立新的canonical类型身份；类型、case索引、payload索引和布局元数据必须来自相互一致且仍有效的上层对象。raw地址、视图指针、借用值和借用字符串不会自行获得GC根、pin或跨暂停帧有效期。上层读栈/变量只负责选择当前帧和组织句柄；Snapshot的三层适配仍属于后续独立39项，不获得本批审查信用。

读取和物化并非纯粹无分配。primitive直接初始化不先清理任意旧值，调用方必须提供合适输出；Value_Copy会按自身所有权规则处理旧目标，也可能clone分配或Throw，不能用于无条件别名覆盖。case/payload写入、引用扫描和GC屏障各有层级：对valueObject执行的屏障不等于对接收inline carrier建立所有引用保证。BOOL失败、void setter与非局部Throw不共享统一回滚协议；失败后不得默认所有输出归零、所有暂存资源释放或整个carrier保持原样。

临时ExplicitIgnoredRoot租约和显式IgnoredRoot不同。既有BUG的静态完整链为有效paused公共求值允许legacy、正式解析circle +缺右操作数失败后回退，legacy左侧先对有效inline Circle布局物化carrier并设置显式Ignore，后续RHS失败未发布该值，也没有对应Unignore清理；GC遍历跳过显式根，直到其他实际路径解除该状态或VM回收。其他图逃逸/写屏障发布路径可能Unignore，不能声称每次成功公共Evaluate都永久滞留。本轮不做动态根滞留复现、泄漏量测或修复。

大型raw struct payload的sizeof(Value)启发式仍保留TODO。完整编译器布局到frame内联存储的可达组合尚未闭合；需从当前type_layout、case payload offsets、分配长度及read/write/copy入口核查，不能把疑点升级为已证越界BUG。相关TODO/BUG在逐项台账保留触发边界与具体核查入口。

源码只替换C中10行原注释并在EOF追加39段Doxygen，H仅EOF追加13段。9024个程序atom（C7032/H1992）及82个字符串/字符字面量（69/13）的拼写、原物理行和UTF8字节列三元坐标相同；先精确移除后缀、逆向10行注释可恢复原始文件。C注释长度变化会改变后续绝对偏移，不声称完整raw前缀或绝对偏移一致；H原28984字节/578LF前缀精确保留。该证明是有限正则投影和raw逆向，不是编译器AST/lexer等价证明。31个原whole（30当前相同、1仅Compat36完整EOF逆向桥）及967个EOL锚支持合同；借用文件不获整模块审查信用。

在实际checkout复用E盘clang-cl构建目录，当前FileAPI递归13target、963源文件和922个首方include头文件作有限守卫。修改前后均build成功，直接无参数运行zr_vm_debug_variable_child_shape_test的全部3个有序既有用例：per_value_child_shape_metadata、expands_union_variant_payloads及semantic_summary_uses_closure_captures。两阶段build/test均退出0、3PASS/0FAIL/0IGNORE，原stdout和stderr完全相同，无归一化。

既有测试覆盖变量形状、Union payload展开及closure集成，并含公共Evaluate对特定Union表达式的拒绝断言；不等于circle +的正向legacy根滞留复现，也不覆盖全部35函数分支、大raw payload、OOM/Throw注入、GC滞留或竞态。输入程序/sourceLabel在内存中，无磁盘zr夹具；独占cwd起初不存在，前后均为空。正常尾部join线程、关闭client、Stop agent、释放function和state；空tearDown不保证Unity长跳转或非局部退出仍清理。CTest当前无显式TIMEOUT，900秒build及120秒直接测试是本任务watchdog；不授CTest调度、WSL或全仓通过信用。

inventory只把C文件从pending更新reviewed；H仅更新真实哈希和13/89部分审查说明，余76项继续pending。台账742行中的其余686行保持raw字节，48个外来inventory行85格及4个已有外来暂存路径保留。已有launch workflow文档只更正两处debug_union.c目录引用，不改其余合同正文；其他旧路径继续由对应模块处理。510叶43979完整键总数不变。

实际配对证据：debug-union-inspection-current-native-pair-r1.json，SHA256 341bd237ce97c12918214e64019a0fa47c7c779f708f1609da114c3f0d30da9f；源码应用：debug-union-inspection-current-source-application-r1.json，SHA256 8bb519fa9e91f275d4b35e579b8f1b5bc43bf80c729059eb5e9b3ff679be3315。
