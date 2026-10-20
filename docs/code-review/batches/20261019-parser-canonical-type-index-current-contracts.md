---
related_code:
  - zr_vm_parser/src/zr_vm_parser/types/canonical_type_index.c
  - zr_vm_parser/src/zr_vm_parser/types/canonical_type.c
  - zr_vm_parser/src/zr_vm_parser/types/canonical_type_lifecycle.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic.c
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_core/include/zr_vm_core/array.h
  - zr_vm_core/src/zr_vm_core/exception/exception.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/types/canonical_type_index.c
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
tests:
  - tests/parser/test_canonical_type_graph.c
  - tests/CMakeLists.txt
plan_sources:
  - user: 全量首方审查、按需补充中文职责和生命周期注释
doc_type: module-detail
---

# 规范类型索引合同

本批逐项审查当前 types/canonical_type_index.c 的10个既有单元：9函数及重复插入不变量1项，9项commented、1项既有TODO。补充9段完整中文Doxygen，覆盖职责、调用时机、前提、资源所有权、失败状态和生命周期，包含16个方向参数、4个非void返回和5个void完成状态及References。台账将10个旧flat路径逐一迁到实际types路径，不新增定义或审查信用。整文件10项均有最终结论，inventory将C标为reviewed；叶内另一个Find记录原raw保留。canonical_type.h仅更正两行既有注释镜像，仍pending且不计任何头文件单元或整文件完成信用。

索引只按structuralHash分桶，再由上层比较完整结构。bucketCount必须是可表示的非零二次幂，hashNext长度与已发布节点配合；碰撞候选不等于类型相等。生产store入口取得追加前的length作为新索引、追加节点后单次Insert。Insert正常false只表示显式前置检查未通过，不回滚外层已追加节点；分配失败另经MEMORY_ERROR离开。超过四分之三负载时先建立新桶，再重写所有next，最后释放旧桶；新桶构造失败前旧链仍由context持有，不把整个外层过程描述为事务回滚。

初始化先有state、canonicalTypes，再建桶和next。索引存储属于context，不能用Init覆盖仍有资源的旧索引；第二个数组构造失败时不会在函数内撤销已建立的桶。Reset由节点payload释放和节点池清空之后调用，只恢复哨兵及逻辑长度、保留容量；Free在关闭节点池后释放索引原生存储，不代管节点payload。First/Next返回当前轮的借用节点索引与碰撞链候选，Reset、Free或链修改后旧遍历状态不可继续；同一context不允许并发或重入修改。

旧OOM BUG判断曾把数组分配失败解释为普通继续执行。当前完整路径为Array_TryInit/扩容失败后由Array_Init或Push传播Throw(MEMORY_ERROR)，随后由同线程恢复帧接管或终止，并无所声称的“失败返回后继续NULL写入”证据。本批据实际调用链撤回该误报，修改C的4行旧BUG注释及H的903/918两行镜像，另收窄C第117行原重复插入TODO；不做行为修复，也不将尚未验证的异常恢复、部分构造清理称作已证实缺陷。其余原始行和EOL保持。

重复插入TODO保留同一家族：当同一nodeIndex在不触发重建的普通连链分支再次Insert时，可能把next指向自身。当前唯一生产追加入口只发布新节点一次；后续核查入口为Index_Insert的普通连链分支与canonical_type_store，决定公开接口是否应拒绝或幂等。重建会重写全部next，不把重复插入在所有分支均形成环写成已证BUG。没有新增BUG/TODO问题家族。

独立审查接受原F，minimum_revision为空。C仅更正5行既有注释后追加9571B169LF的9段Dox，8280B199LF成为17893B368LF；H仅更正2行既有注释，52758B935CRLF成为52746B935CRLF。C885/H1705程序atom及2/4字符串或字符字面量的拼写、物理行与UTF8字节列相同，绝对字节偏移仅按前置注释长度差逐一映射；其他原始行、顺序与EOL逐字保持，按逆操作可精确还原批前raw。Git只将H的统一CRLF规范化为LF，未借此混入既有程序改动。这是有限正则投影，不是编译器lexer/AST或运行时等价证明。

复用实际checkout的E盘clang-cl构建目录，前后各构建一次，无参数、无过滤地直接运行既有canonical_type_graph主程序。每阶段19PASS/0FAIL/0IGNORE，4个native子进程全部await退出0，测试顺序及stdout/stderr原始字节一致，无归一化；19个不同既有用例共38次PASS实例。FileAPI核实16个递归target、981源文件、936首方include头文件，共1917个唯一有限输入。仅两处受控注释投影回原raw，其余输入逐字保持。900秒build、600秒test为本任务watchdog，未触发；不冒充既有CTest显式TIMEOUT。

既有测试涉及结构驻留、完整函数契约、泛型、非法值拒绝、Reset、十万类型大图和深层类型；这些是当前回归证据，不等于19项均仅测索引或覆盖9函数所有分支。未强制OOM、未运行重复Insert复现或异常恢复、并发、GC、WSL及全仓验收，不新增重复实现的测试。

原F、I及Root最小修订证据保留不可变。首次native登记中Root的RAM批量替换误把source map的值写成F针，已在任何native/source执行前按原16目标完整图重建981个真实输入针，并在artifacts.json保留错误针、修正针及实际进程退出证据；未使用错误基线运行构建。两阶段独占cwd初始不存在、结束为空；提交后只移除本批精确归属的临时索引及空cwd，保留共享构建、缓存和必要失败证据。未创建源码副本。

本批只纳入2个注释源文件与本模块叶、批次文档、inventory和导航共6个路径。全仓510叶、43979个唯一完整键不增不减，无重复；inventory为1802 reviewed/2459 pending，48个外来inventory行85格及4个外来暂存路径保持。该C文件完成不代表全仓完成。

实际配对证据：parser-canonical-index-current-native-pair-r1.json，SHA256 dc52d4c21c2e0524e8a7ce436aec327957d242b1813cf332cfdc3b3db4e50241；源码应用：parser-canonical-index-current-source-application-r1.json，SHA256 1f59c6eeaf7bf30bfec59f8d7a2d0401cf7a7550702280045691b44e70046917。
