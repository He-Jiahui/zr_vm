---
related_code:
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
  - zr_vm_lib_container/include/zr_vm_lib_container/module.h
  - zr_vm_lib_container/src/zr_vm_lib_container/contiguous_view.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_memory.c
implementation_files:
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
plan_sources:
  - user: 2026-10-04 全文件调用链与中文契约注释审查
  - docs/code-review/comment-standard.md
tests:
  - tests/container/test_container_runtime.c
  - tests/container/container_test_common.c
  - tests/iterator/test_enumerator_protocol.c
  - tests/container/test_pooling_closed_type_runtime.c
  - tests/library/test_official_provider_convergence.c
doc_type: module-detail
---

# zr.container 原生模块与反射注册契约

本页描述 module.c 当前静态调用链；不宣称本轮运行测试、GC、OOM、ABI或性能通过。
共享 module.c 已有161函数及全部状态、字段、枚举、注册项与非显然块的357项语义台账；
内部类型和部分函数仍须补齐当前文档级注释规范，该文件继续为 pending。
公开 module.h 此批补齐12项完整契约；它的验收不扩大为内部实现或相关依赖全文件已审。

## 宿主公开 C 接口

公开头部将描述符查询、VM 登记和诊断窗口分开说明。六个接口的签名及构建条件均不变。

| 入口 | 调用目的 | 使用边界 |
| --- | --- | --- |
| GetModuleDescriptor | 检查 container 类型、泛型与协议；迭代协议测试直接查询。 | 不创建 VM/容器、不登记依赖；返回 provider 的 const 静态借用数据。 |
| GetPoolingModuleDescriptor | 检查池的类型和稳定槽回调角色。 | 仅查询；pooling 的模块链接引用 container Span，VM 使用前仍须满足依赖。 |
| Register | CLI 标准模块启动链登记 iteration、container、pooling，再安装基本 ARRAY 迭代桥接。 | global/mainThreadState/基本ARRAY原型须有效，registry由登记链按需Attach；失败保留前序登记，测试 state 工厂忽略 bool，不能替代成功检查。 |
| Debug_ResetHotMapLookupStats | 在待测脚本前开始新的进程计数窗口。 | 只清诊断数字，不清真实缓存；非 DEBUG 无动作，不暂停其它线程。 |
| Debug_GetHotMapLookupStats | 脚本运行后按值取得整数快照。 | 非 DEBUG 全零；当前 hotHitCount/memberVersionHitCount 同时递增，entryValidationReadCount 没有写入，不能据此证明查询结果正确。 |
| ZrVm_GetNativeModule_v1 | 共享 provider 提供固定 ABI 导出符号。 | 仅共享构建；加载器查询/验证后登记，不在此执行宿主 Register 的依赖链或基本 ARRAY 适配。 |

实际入口为 `zr_vm_cli/src/zr_vm_cli/project/project.c:368`、
`tests/iterator/test_enumerator_protocol.c:132`、`tests/container/test_pooling_closed_type_runtime.c:21`、
`tests/container/test_container_runtime.c:1535`/`tests/container/test_container_runtime.c:1539` 和
`zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:383`。
描述符内表、字符串和回调借用 provider 存储；provider 必须活到最后一次消费。
统计快照不持有 VM 对象，进程计数与并行更新的同步/窗口隔离保留具体 TODO。
Register 的同名字段 presence 策略和双 ignore 扩容失败 BUG 沿下文完整静态链说明；本次不改变行为。

## 数据与生命周期

Array的隐藏items和Map/Set的隐藏entries是GC原生数组；公开length/count/capacity
与真实backing分开。capacity是可写增长元数据，不等于已保留物理空间。
Map字符串键缓存只保留成功位置：global cacheIdentity、entries身份、memberVersion
与键对象身份一致才允许命中。缓存裸Pair/key/backing指针不提供独立GC根。
Map/Set当前仍扫描entries；IHashable/IEquatable约束不承诺哈希O(1)复杂度。

字段字符串与迭代原型永久标记，只在global域存活期间有效。Enumerator实例的source
字段保持backing/list可达，current复制当前值，index/nextNode保存游标；没有长度或
修改版本快照。LinkedNode摘链后清链接却保留value，外部节点引用仍能保活该值。
结果值复制遵守Value_Copy目标初始化及ownership覆盖语义；normalized普通值才能
走浅写分支，GC对象仍须写屏障。

Pair是普通STRUCT，不能从可写first字段推出迭代current与Map entry共享对象。
Span两型是非拥有RefLike STRUCT视图；source/start/length角色共享，区间与来源
检查在contiguous_view实现，不能把GET只读receiver或Map省结果派发当容器不可写。

## 八类型与全部公开字段

| 类型 | 公开字段 | 类型/泛型/协议 |
|---|---|---|
| Array<T> | length:int、capacity:int | CLASS；T无额外约束；ARRAY_LIKE、ITERABLE |
| Map<K,V> | count:int | CLASS；K需IHashable/IEquatable<K>、V无附加约束；ITERABLE<Pair<K,V>> |
| Set<T> | count:int | CLASS；T需IHashable/IEquatable<T>；ITERABLE<T> |
| Pair<K,V> | first:K、second:V | STRUCT；K/V无附加约束；EQUATABLE、COMPARABLE、HASHABLE |
| LinkedList<T> | count:int、first/last:LinkedNode<T> | CLASS；T无额外约束；ITERABLE |
| LinkedNode<T> | value:T、next/previous:LinkedNode<T> | CLASS；T无额外约束；protocolMask=0 |
| Span<T> | source:object、start:int、length:int | STRUCT；REF_LIKE、CONTIGUOUS_VIEW_MUTABLE |
| ReadOnlySpan<T> | source:object、start:int、length:int | STRUCT；REF_LIKE、CONTIGUOUS_VIEW_READONLY |

逻辑字段共18项，Span两型复用三项描述符，因此不同声明为15项。字段宏默认可写；
ReadonlySpan不提供SET_ITEM，不能从共享字段宏推导来源数组被永久冻结。
Array/Map/Set/Pair/List/Node允许值与装箱构造；Span两型仅允许值构造、禁止装箱。

## 注册、参数范围与派发

Register要求已初始化主线程/基本原型，按iteration→container→pooling→内建array
getIterator适配顺序执行&&，失败不回滚前面已成功登记。CLI传播bool；测试state
工厂忽略该bool，不能用创建state代替注册成功证据。描述符/数组/回调均借用provider
静态存储；插件必须在最后一个consumer结束后才卸载。共享v1入口仅返回描述符，
不隐式执行Register或安装array适配。

methods元数据创建回调闭包后由callback(context,result)派发；meta表独立消费，
types由materialize逐项添加，字段由fieldCount循环注册。构造min/max：Array0..1，
Map/Set/LinkedList0..0，Pair0..2但回调只允许0或2（拒绝1），LinkedNode0..1，
Span/ReadOnlySpan0..0。普通方法的精确参数数和返回类型见本页末尾注册表。
Array GET_ITEM恰1参并READONLY_RECEIVER；SET_ITEM恰2参默认flags0。
Map GET恰1参，使用STACK_ROOT_CONTEXT、NO_SELF_REBIND、INLINE_VALUE_CONTEXT、
RESULT_ALWAYS_WRITTEN、READONLY_INLINE_VALUE_CONTEXT、READONLY_RECEIVER。
Map SET恰2参，前五项相同，但用RESULT_OPTIONAL，未标READONLY_RECEIVER；可省结果
仍修改内容。inline缺backing返回false由无线程异常派发转null；缺键自身写null/true。
普通getter也可在ensure/search返回NULL时null/true，线程异常由外层检查。
Span两型GET恰1参且READONLY_RECEIVER；可写Span SET恰2参默认flags0。

## 已证静态缺陷与待核项

BUG：合法Pair<int,int>两个first取2^53和2^53+1、second相同，经注册compareTo或
COMPARE进入values_compare。核心精确整数相等为false，但double转换相等，最终
两个方向都返回1；UInt64还经nativeInt64解释。完整合法静态链支持BUG，本轮无复现。

BUG：适配已有初始化global且array原型无getIterator，ignored表满、原型未ignored。
permanent标记不等于ignore登记。宿主允许之前注册、closure及其他分配，只按阶段
拒绝getter pin和setter pin的两次ignore ARRAY扩容；getter返回NULL且表仍满，
installer继续创建closure成功，再setter void早退未写字段，installer/Register仍true。
真实wrapper由GlobalState_New安装，execution_memory.c上游allocator非零申请NULL
直接返回，不抛错误。只拒绝一次setter扩容不能构成完整链，因为getter先成功可能
已经扩表；这里限定两次阶段拒绝，没有运行故障注入或修改行为。

TODO：process静态缓存跨mutator/多域交错同步与moving GC裸指针更新；公开/动态
标量元数据可否合法带ownership；Array/Set clear复用池未逐槽release的合法元素
限制与最终析构；Pair字段复制/写入与entries版本关系；generic移位中间失败传播；
capacity倍增上界；链表多setter部分失败状态；原型先发布后闭包装配异常恢复；
capacity构造参数仍叫index；已有同名非callable字段被当适配已完成。核查入口均在
本源相邻TODO，未闭合风险不升级BUG、也不当作已验证运行结论。

## 逐项注册用途（当前静态表）

| 注册项 | 用途 | 本项约束 |
|---|---|---|
| kArrayMethods[macro-1] | 绑定 Array.span 到 ContiguousView_FromArray，返回引用当前数组区间的 Span<T>。 | 实例无参；VIEW_CREATE 角色供连续视图 lowering 使用；不复制/拥有元素，区间与来源由视图实现检查。 实际绑定 ZrVmLibContainer_ContiguousView_FromArray。 |
| kArrayMethods[macro-2] | 绑定 Array.add，把 value:T 追加到 backing 并更新公开 length。 | 实例恰1参，默认派发flags0；普通 null 结果，追加/元数据失败不提供事务回滚。 实际绑定 zr_container_array_add。 |
| kArrayMethods[macro-3] | 绑定 Array.insert，在 index 处移位后插入 value:T。 | 实例恰2参 index:int/value:T，位置0..length；普通 null 结果，generic移位逐次失败传播见 TODO。 实际绑定 zr_container_array_insert。 |
| kArrayMethods[macro-4] | 绑定 Array.removeAt，移除指定现存位置并更新 length。 | 实例恰1参 index:int；普通 null 结果，非法位置或存储失败走当前边界错误路径。 实际绑定 zr_container_array_remove_at。 |
| kArrayMethods[macro-5] | 绑定 Array.clear，复用存储并将 length 置零。 | 实例无参，普通 null 结果；capacity 保留，快速清空未逐槽 release，合法owner元素限制待核。 实际绑定 zr_container_array_clear。 |
| kArrayMethods[macro-6] | 绑定 Array.contains，按当前元素相等规则返回存在性。 | 实例恰1参 value:T，普通 bool 结果；raw-int可专用搜索，对象equals可执行用户代码。 实际绑定 zr_container_array_contains。 |
| kArrayMethods[macro-7] | 绑定 Array.indexOf，返回首个相等位置或 -1。 | 实例恰1参 value:T，普通 int 结果；不假定equals无副作用，不搜索未在backing中的公开length虚值。 实际绑定 zr_container_array_index_of。 |
| kArrayMethods[macro-8] | 绑定 Array.getIterator，为 Iterable<T> 创建 backing Enumerator。 | 实例无参，ITERABLE_INIT 角色；返回GC迭代对象，其source保活backing，没有长度/版本快照。 实际绑定 zr_container_array_get_iterator。 |
| kArrayMetaMethods[1] | 绑定 Array<T> 构造器，建立空 backing 与指定公开容量。 | 0..1参，返回已初始化Array<T>；可选参数实际capacity>=0，但元数据复用index名称；本项无GET_ITEM只读flag。 实际绑定 zr_container_array_constructor。 |
| kArrayMetaMethods[2] | 绑定 Array GET_ITEM，从有效下标复制元素，越界返回 null。 | 恰1参 index:int，READONLY_RECEIVER；结果目标由绑定层初始化，复制元素保留Value_Copy ownership语义。 实际绑定 zr_container_array_get_item。 |
| kArrayMetaMethods[3] | 绑定 Array SET_ITEM，把 value:T 写到现存 index 并返回该值。 | 恰2参 index:int/value:T，默认flags0；越界错误，不允许本元方法扩长度，结果复制按值ownership。 实际绑定 zr_container_array_set_item。 |
| kMapMethods[macro-1] | 绑定 Map.containsKey，按实际entries查找K并返回存在性。 | 实例恰1参 key:K，普通bool结果；K受Map泛型哈希/相等约束，字符串正命中可缓存。 实际绑定 zr_container_map_contains_key。 |
| kMapMethods[macro-2] | 绑定 Map.remove，删除首个匹配键并同步count。 | 实例恰1参 key:K，普通bool结果；缺键或存储移除失败为guest false，callback状态另由派发消费。 实际绑定 zr_container_map_remove。 |
| kMapMethods[macro-3] | 绑定 Map.clear，清空并复用entries、重置count。 | 实例无参，普通null结果；不逐Pair字段析构，保留空间，版本失效由清空backing实现。 实际绑定 zr_container_map_clear。 |
| kMapMethods[macro-4] | 绑定 Map.getIterator，供 Iterable<Pair<K,V>> 枚举条目。 | 实例无参，ITERABLE_INIT角色；返回GC迭代器引用entries，current的Pair复制遵循STRUCT语义而非保证别名。 实际绑定 zr_container_map_get_iterator。 |
| kMapMetaMethods[1] | 绑定 Map<K,V> 无参构造，建立entries及零count。 | 恰0参，返回完成初始化的Map；本项默认flags0，没有GET/SET的RESULT_OPTIONAL或栈根快路径flag。 实际绑定 zr_container_map_constructor。 |
| kMapMetaMethods[2] | 绑定 Map GET_ITEM 普通getter及只读内联快读，返回V或null。 | 恰1参 key:K；STACK_ROOT_CONTEXT/NO_SELF_REBIND/INLINE_VALUE_CONTEXT/RESULT_ALWAYS_WRITTEN/READONLY_INLINE_VALUE_CONTEXT/READONLY_RECEIVER；inline缺backing false由派发转换，缺键自身null/true。 实际绑定 zr_container_map_get_item, zr_container_map_get_item_readonly_inline_fast。 |
| kMapMetaMethods[3] | 绑定 Map SET_ITEM 普通setter及省结果内联写，更新或新增K/V条目。 | 恰2参 key:K/value:V；STACK_ROOT_CONTEXT/NO_SELF_REBIND/INLINE_VALUE_CONTEXT/RESULT_ALWAYS_WRITTEN/READONLY_INLINE_VALUE_CONTEXT/RESULT_OPTIONAL；无READONLY_RECEIVER，result可省略，写入仍改变内容。 实际绑定 zr_container_map_set_item, zr_container_map_set_item_readonly_inline_no_result_fast。 |
| kSetMethods[macro-1] | 绑定 Set.add，按相等规则检查重复后新增T。 | 实例恰1参 value:T，普通bool结果；重复为guest false、callback true，新增后更新count，失败无事务回滚。 实际绑定 zr_container_set_add。 |
| kSetMethods[macro-2] | 绑定 Set.contains，在线性entries中查找T。 | 实例恰1参 value:T，普通bool结果；T的哈希/相等约束不等于此实现按元素哈希O(1)查找。 实际绑定 zr_container_set_contains。 |
| kSetMethods[macro-3] | 绑定 Set.remove，删除首个相等T并同步count。 | 实例恰1参 value:T，普通bool结果；缺失或存储移除失败为guest false，generic位移失败传播待核。 实际绑定 zr_container_set_remove。 |
| kSetMethods[macro-4] | 绑定 Set.clear，复用entries空间并清零count。 | 实例无参，普通null结果；快速clear不逐值release，owner元素可达限制与析构需核查。 实际绑定 zr_container_set_clear。 |
| kSetMethods[macro-5] | 绑定 Set.getIterator，供 Iterable<T> 枚举实际entries。 | 实例无参，ITERABLE_INIT角色；GC迭代器source保持backing可达，不捕获修改版本。 实际绑定 zr_container_set_get_iterator。 |
| kSetMetaMethods[1] | 绑定 Set<T> 无参构造，建立entries和零count。 | 恰0参，返回Set<T>，默认flags0；T哈希/相等约束在type泛型表，本项没有下标元方法。 实际绑定 zr_container_set_constructor。 |
| kPairMethods[macro-1] | 绑定 Pair.equals，确认同owner Pair再比较两个分量。 | 实例恰1参 other:Pair<K,V>，普通bool结果；对象equals可回调，K/V没有本表新增约束。 实际绑定 zr_container_pair_equals。 |
| kPairMethods[macro-2] | 绑定 Pair.compareTo，按first后second字典序比较。 | 实例恰1参 other:Pair<K,V>，普通int结果；与COMPARE共用pair_compare，合法int64数字回退有已记录BUG。 实际绑定 zr_container_pair_compare。 |
| kPairMethods[macro-3] | 绑定 Pair.hashCode，组合两个分量的哈希供IHashable使用。 | 实例无参，普通int结果；用户hashCode可能置线程异常，本回调仍true，由外层派发检查。 实际绑定 zr_container_pair_hash_code。 |
| kPairMetaMethods[1] | 绑定 Pair<K,V> 结构构造，初始化first/second。 | 元数据0..2参 first:K/second:V，回调只允许0或2并拒绝1；返回普通STRUCT值，复制不保证共享对象身份。 实际绑定 zr_container_pair_constructor。 |
| kPairMetaMethods[2] | 绑定 Pair COMPARE 到与compareTo相同的字典序回调。 | 恰1参 other:Pair<K,V>，普通int结果，默认flags0；数值回退BUG可经该合法元方法到达。 实际绑定 zr_container_pair_compare。 |
| kLinkedListMethods[macro-1] | 绑定 LinkedList.addFirst，新建含T的节点并连接为首节点。 | 实例恰1参 value:T，返回GC LinkedNode<T>；node.value保留元素，失败可能已有部分链接修改。 实际绑定 zr_container_linked_list_add_first。 |
| kLinkedListMethods[macro-2] | 绑定 LinkedList.addLast，新建含T的节点并连接为尾节点。 | 实例恰1参 value:T，返回GC LinkedNode<T>；外部node引用可在摘链后继续保活value。 实际绑定 zr_container_linked_list_add_last。 |
| kLinkedListMethods[macro-3] | 绑定 LinkedList.removeFirst，复制首节点value再摘链。 | 实例无参，返回T或空表null；Value_Copy处理结果ownership，摘链不清空node.value。 实际绑定 zr_container_linked_list_remove_first。 |
| kLinkedListMethods[macro-4] | 绑定 LinkedList.removeLast，复制尾节点value再摘链。 | 实例无参，返回T或空表null；先复制值，后摘链接/count，多个setter并非事务。 实际绑定 zr_container_linked_list_remove_last。 |
| kLinkedListMethods[macro-5] | 绑定 LinkedList.remove，沿next删除首个相等value。 | 实例恰1参 value:T，普通bool结果；用户equals可运行代码，遍历没有版本快照或原子删除保证。 实际绑定 zr_container_linked_list_remove。 |
| kLinkedListMethods[macro-6] | 绑定 LinkedList.clear，断开节点链接并重置首尾/count。 | 实例无参，普通null结果；保留node.value，外部node持有者仍可达元素，失败可部分清链。 实际绑定 zr_container_linked_list_clear。 |
| kLinkedListMethods[macro-7] | 绑定 LinkedList.getIterator，为Iterable<T>建立nextNode游标。 | 实例无参，ITERABLE_INIT角色；source保持list可达，之后跟随当前节点链接，未提供修改检测。 实际绑定 zr_container_linked_list_get_iterator。 |
| kLinkedListMetaMethods[1] | 绑定 LinkedList<T> 空表构造，first/last为空且count为零。 | 恰0参，返回GC LinkedList<T>，默认flags0；本项不分配节点或元素backing。 实际绑定 zr_container_linked_list_constructor。 |
| kLinkedNodeMetaMethods[1] | 绑定 LinkedNode<T> 构造，初始化value和空双向链接。 | 0..1参 value:T，省略时value为空；返回GC LinkedNode<T>，脱链不等于释放value。 实际绑定 zr_container_linked_node_constructor。 |
| kSpanMethods[macro-1] | 绑定 Span.slice 到ContiguousView_Slice，创建同源相对区间。 | 实例恰2参 start:int/length:int，VIEW_SLICE角色；返回Span<T>，来源及合法边界由视图回调验证，不复制元素。 实际绑定 ZrVmLibContainer_ContiguousView_Slice。 |
| kSpanMethods[macro-2] | 绑定 Span.asReadOnly，保留同源区间并转换只读视图类型。 | 实例无参，READONLY_VIEW_CONVERSION角色；返回ReadOnlySpan<T>而非冻结原数组，来源生命周期仍须满足借用约束。 实际绑定 ZrVmLibContainer_ContiguousView_AsReadOnly。 |
| kReadOnlySpanMethods[macro-1] | 绑定 ReadOnlySpan.slice，切分同源只读区间。 | 实例恰2参 start:int/length:int，VIEW_SLICE角色；返回ReadOnlySpan<T>，无转回可写视图接口。 实际绑定 ZrVmLibContainer_ContiguousView_Slice。 |
| kSpanMetaMethods[1] | 绑定 Span<T> 空构造到ContiguousView_Construct，重置空区间。 | 恰0参，返回RefLike Span<T>，默认flags0；type允许值构造但禁止装箱，不获得元素所有权。 实际绑定 ZrVmLibContainer_ContiguousView_Construct。 |
| kSpanMetaMethods[2] | 绑定 Span GET_ITEM，按相对index读取来源元素。 | 恰1参 index:int，READONLY_RECEIVER；返回T，实际区间/来源检查在ContiguousView_GetItem，本flag不禁止Span SET_ITEM。 实际绑定 ZrVmLibContainer_ContiguousView_GetItem。 |
| kSpanMetaMethods[3] | 绑定 Span SET_ITEM，按相对index修改来源并返回赋值结果。 | 恰2参 index:int/value:T，默认flags0；来源数组或lease有效性和边界由ContiguousView_SetItem校验，视图不拥有元素。 实际绑定 ZrVmLibContainer_ContiguousView_SetItem。 |
| kReadOnlySpanMetaMethods[1] | 绑定 ReadOnlySpan<T> 空构造到公共视图构造回调。 | 恰0参，返回RefLike ReadOnlySpan<T>，允许值构造禁止装箱；本项不声明SET_ITEM。 实际绑定 ZrVmLibContainer_ContiguousView_Construct。 |
| kReadOnlySpanMetaMethods[2] | 绑定 ReadOnlySpan GET_ITEM，按相对index读取来源元素。 | 恰1参 index:int，READONLY_RECEIVER；返回T，来源可由其他持有者修改，本类型无SET_ITEM而非复制冻结快照。 实际绑定 ZrVmLibContainer_ContiguousView_GetItem。 |
| g_container_types[1] | 声明Array<T>类，提供索引和T迭代接口，连接Array字段/普通方法/元方法表。 | 单T无额外约束；CLASS、允许值与装箱构造，ARRAY_LIKE/ITERABLE协议；length角色用于索引长度，隐藏items由回调维护。 |
| g_container_types[2] | 声明Map<K,V>类，迭代Pair<K,V>并提供键下标读写。 | K受IHashable/IEquatable<K>约束、V无附加约束；CLASS、允许值与装箱构造、ITERABLE协议；count可见但实际长度取entries。 |
| g_container_types[3] | 声明Set<T>类，提供去重元素操作及T迭代。 | T受IHashable/IEquatable<T>约束；CLASS、允许值与装箱构造、ITERABLE协议；无下标元方法，当前查询线性扫描。 |
| g_container_types[4] | 声明Pair<K,V>普通结构，提供分量、相等、排序及哈希方法。 | K/V无附加泛型约束；STRUCT、允许值与装箱构造，EQUATABLE/COMPARABLE/HASHABLE协议；无RefLike位，值复制不能当共享别名。 |
| g_container_types[5] | 声明LinkedList<T>类，提供首尾节点操作和T迭代。 | 单T无额外约束；CLASS、允许值与装箱构造、ITERABLE协议；GC节点链接维护count/首尾，不使用entries数组。 |
| g_container_types[6] | 声明LinkedNode<T>类，保存value及双向链接。 | 单T无额外约束；CLASS、允许值与装箱构造、protocolMask0；零或一参构造，脱链后value仍可由外部节点引用保活。 |
| g_container_types[7] | 声明可写Span<T>连续非拥有视图，接入区间字段与slice/read/write回调。 | 单T无额外约束；STRUCT、允许值构造禁止装箱，REF_LIKE/CONTIGUOUS_VIEW_MUTABLE协议；source/start/length来源与边界须有效。 |
| g_container_types[8] | 声明ReadOnlySpan<T>连续非拥有只读视图，复用区间字段并接入slice/read回调。 | 单T无额外约束；STRUCT、允许值构造禁止装箱，REF_LIKE/CONTIGUOUS_VIEW_READONLY协议；无SET_ITEM，不冻结其他持有者可写来源。 |

## 四种宿主缓存与登记入口的当前契约

前批28项、字段名/缓存入口40项和静态状态/枚举/块21项均已提交；本批补齐迭代原型/状态/块及消费者14项，累计103项。
357项台账保留，剩余254项待当前契约及完整性复核；数字引用迁移及重审旧BUG不增加完成数。

| 类型 | 调用目的 | 借用与失效边界 |
| --- | --- | --- |
| ZrContainerFieldStringCacheEntry | 为固定热名以外的宿主字段文本保存本域永久字符串，避免每次访问重建键。 | fieldName 不复制，NUL文本必须活到缓存清空；fieldString 不接管域寿命；identity切换清槽。 |
| ZrContainerHotFieldStringCache | 为 backing、Pair、Enumerator 的八种热字段角色复用永久键。 | 缓存的是键名，不是字段值或容器GC根；按本类型 accessor 的 GlobalState身份失效。 |
| ZrContainerHotMapLookupCacheSlot | 保存成功字符串键扫描的位置，供同一 backing/版本/对象身份的候选命中。 | key/Pair都是裸借用；负查找不入槽；版本相同不独立证明 Pair.first未变或下标合法。 |
| ZrContainerHotMapLookupCache | 合并最近成功命中与四候选槽，减少 Map重复扫描。 | entries或域切换清 MRU/槽/游标；已有同key/version覆盖不推进，空槽插入与满槽轮换均推进。 |

已 MarkAsPermanent 的字段字符串由永久区域/根保活且不被 moving GC搬迁，依据
`zr_vm_core/src/zr_vm_core/gc/gc_object.c:1210` 和 `zr_vm_core/src/zr_vm_core/gc/gc_compact.c:217`。
Map 的普通 entries/key/Pair缓存没有这项标记或独立GC根；不能跨分配、用户回调或GC继续持有旧裸地址。
同一种 cacheIdentity 仅由对应 accessor 比较/更新；它是生命周期标量，不是同步锁或内容版本。

TODO: 多 GlobalState/mutator交错访问进程静态缓存的同步需沿 `zr_vm_lib_container/src/zr_vm_lib_container/module.c:591` 核查。
TODO: Map裸地址重写与 moving GC协作需沿 `zr_vm_lib_container/src/zr_vm_lib_container/module.c:1068` 及 gc_cycle 的对象重写入口核查；
Pair.first 的合法写入与 entries.memberVersion 联动需沿 `zr_vm_lib_container/src/zr_vm_lib_container/module.c:2650` 核查，不能由相同版本推出键内容未变。

宿主 Register 的顺序是 iteration→container→pooling→基本ARRAY适配。iteration链按需 Attach registry，
所以调用者无需提前创建 registry；global/mainThreadState/基本ARRAY原型必须已有效。后阶段失败不回滚
前阶段成功登记。CLI `zr_vm_cli/src/zr_vm_cli/project/project.c:368` 检查 bool，`tests/container/container_test_common.c:60` 忽略 bool，
该测试工厂调用本身不能证明登记成功。描述符查询及共享插件 getter 不执行这条宿主初始化链。

TODO: `zr_vm_lib_container/src/zr_vm_lib_container/module.c:5814` 的同名字段 presence检查不验证 callable/getIterator协议；核查已有字段的实际消费者。
BUG: 合法初始化global、ARRAY原型尚未ignore且ignored表已满、字段缺失时，宿主分阶段拒绝
getter pin的ignore扩容、允许中间闭包创建、再拒绝setter pin的扩容，void setter早退但Register可仍true。
完整链为 `zr_vm_lib_container/src/zr_vm_lib_container/module.c:5814`→闭包创建→`zr_vm_lib_container/src/zr_vm_lib_container/module.c:5827`；pin入口
`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1429`/`:1385`；
只拒绝一次setter扩容不足以证明该链，先前getter可能已扩表。本轮只有静态证据，无故障注入信用。
实际宿主登记链为 `zr_vm_lib_container/src/zr_vm_lib_container/module.c:5873`，iteration 的 Attach入口是 `zr_vm_lib_iteration/src/zr_vm_lib_iteration/module.c:21`。


## 字段名与缓存入口的40项详细契约

23函数、16字段名常量及1 DEBUG状态逐项核对实际消费点。缓存家族的3 enum、
15函数内static状态及3语句块由下节21项独立审查；该40项批次未覆盖iterator prototype状态；本次14项详见后节。
first区分LinkedList首节点和Pair首分量，同文本键和角色Pair槽不说明共享字段值；
C也不保证等字面量地址合并或分离。fallback借用宿主文本，48槽满走断言后NULL。

永久键由原域保活且域内不搬迁；identity切换清槽，不立即回收仍活旧域字符串。
accessor静态缓存视图不可跨identity切换保存使用，键缓存不保活目标容器/字段值。
Map try依赖刚由accessor核验域，本身不重验cacheIdentity；slot假定非NULL key而未独立检查。
命中仅按对象地址、entries版本及非NULL Pair，不重比Pair.first或检查下标长度。
成功扫描才发布候选，不缓存NULL entry；已有同key/version覆盖不推进，空槽或满槽插入均推进。

两Debug命中计数同点递增，entryValidationReadCount没有递增/非零更新，初始化和Reset写零；Reset不清真实缓存，
Get非DEBUG全零。Map静态查找缓存的AttachedDomain合法并发已由后节源码链标为BUG；其它缓存/统计同步、
moving GC裸地址重写及Pair.first版本联动仍各自TODO，不能扩大本次缺陷结论。公开头16处旧数字引用已按当前语义修正后迁移，其12项既有职责范围保留。
本批仅以字节保护、真实调用、独立review和一TU编译对照验收；module.c继续pending，
不授运行时、GC、OOM、ABI、性能或跨仓库capture信用。


## 静态缓存状态、枚举及边界块21项

本批1匿名enum、2 enumerator、15函数内static和3 meaningful blocks按所属词法函数逐项
复核直接读写及上层消费；旧裸name跨函数混算的cache/identity调用点已更正。不是新增语义单元：
台账仍357项，累计89详细复核，剩268待核；module.c保持pending、H12既有闭合保留。

48是普通字段名fallback容量，固定11名字走专槽，source/current/index/nextNode另有热accessor；
同一identity内无淘汰，fallback借用的宿主指针及NUL文本内容须保持有效且不变直至换域清表。4是Map成功候选槽，另有独立MRU；
查询路径显式slots[0..3]，更改常量须联动数组、查询、取模与TZrUInt8游标。

identity来自GlobalState创建时非零counter。域切换只覆盖静态槽，不析构仍活旧域永久键；
返回块是可覆盖借用视图，不能跨域保存。多域/宿主并行合法入口和同步仍精确TODO；
当前普通OBJECT/ARRAY的minor原地晋升、old compaction地址稳定，缓存不建立根。
未来搬迁及不可达对象回收时的静态裸地址重写/保活协议仍待核，不冒充当前moving-GC BUG。

### BUG: 删除末项后Map成功缓存仍可命中

合法单域串行路径：Map<string,int>构造建立entries，插入Pair走generic storage_set；
remove的find_index先命中或成功扫描填入key/Pair/index/version，然后generic last removal
只HashSet_Remove摘bucket/elementCount，不改entries.memberVersion；self.count写到Map自身。
在无中间GC或其它Map查询覆盖缓存、同一backing和同一字符串对象的窗口，下一containsKey
在真实length扫描前直接true；普通/只读内联GET_ITEM都可读脱链Pair.second。
这一逻辑结果无需推断Pair立即被free；bucket节点与Pair SZrObject分开管理。

示意可检验路径（本轮不执行、不新增实现镜像测试）：

    let container = import("zr.container");
    var key = "k";
    var map = new container.Map<string, int>();
    map[key] = 1;
    var removed = map.remove(key);
    var stillPresent = map.containsKey(key);

remove成功时stillPresent应为false，上述可达窗口当前错误地为true。待后续修复generic
删除时backing版本联动或显式清缓存；本轮不改行为。既有warm-cache/clear测试没有覆盖
remove末项→同对象键contains/get。完整证明锚点由本批static Map状态和backing-reset台账
记录；Pair.first写入的另一个版本联动疑点仍为TODO。

单TU编译复用前批实际r106结果为当前源码基线；只增加新注释后的同配置对照。
数字引用迁移、old unselected checker211保留，不授全台账、runtime、整模块或capture信用。


## 迭代原型、状态与消费者14项

本批9函数、4个词法函数内static和1发布顺序块按当前调用链复核；其中8函数是消费者，
另1函数是runtime prototype。先前89加本14为103/357，254仍待审，C保持pending，H12保持。

五入口：包装Array的items、内建array本身、Map/Set的entries和LinkedList本身。来源字段
只在成功写入后形成实例到来源的可达边；没有独立root handle，也不复制集合或锁定长度/版本。
链表创建时取当时first，后续取节点当时next；数组每步读当前backing。guest null元素仍
能yield true；字段读不到元素的NULL指针与值标签NULL不同。native返回数量1与payload
bool分开，来源为空/负游标可在读取前结束；current经ownership-aware setter发布，字段部分写入不回滚。

两种prototype按精确native函数身份选择，current名字供contract成员读取；永久标记只在
所属GlobalState内保活，换域清槽不释放仍活旧域对象。SetIteratorContract按值复制栈local
contract；核心Object_IterMoveNext/IterCurrent读取contract，不把protocol位作为派发门槛。
核心数组快路可绕过native回调；iterator_runtime.h的IteratorFrame是另一套producer接口。receiver的raw-kind转换检查是调试断言，发布构建依赖调用方布局契约。

TODO: slot先发布后装配closure/contract的异常恢复与重试；ClosureNative_New分配后直接
解引用，调用方NULL分支不证明OOM可恢复。原型并行首填充/切域需独立核查，不能用Map
查找缓存的竞争结论替代。TODO: 无符号异常游标是否可由guest隐藏字段/恢复入口带入。

已读现有测试覆盖固定数组重复/嵌套、raw-int、Map Pair聚合和Set foreach及协议/适配器；
未据这些测试证明LinkedList foreach、来源中途变更可见、迭代器单独保活来源后GC、多域
切换/并发或OOM发布重试。此轮不运行测试，不授runtime/故障注入信用。

### BUG: AttachedDomain多worker竞争Map静态查找缓存

默认ZR_THREAD_LOCK/UNLOCK空钩子下，启用supportMultithread、workerCount=2的attached
ThreadScheduler可以安排两个Job<int>，每个worker只在自己任务中构造/查询自己的Map。
Send限制作用于Job结果T；不需要共享Map对象。worker用同一GlobalState创建不同State，
WorkItem root保活任务/callable；队列锁在ExecutePreparedJob之前释放。MutatorEnter与
NativeEnter在短持协调锁登记状态后释放，绑定lane再执行callback；MutationBegin在无
concurrent major时不持域mutation lock，也不是这些静态槽的同步协议。

因此两个合法独立Map的字符串查询可同时改变同一静态cache的backing、MRU、四槽及游标，
构成C数据竞争，与删除末项导致旧结果的串行BUG分开。已在旧accessor及其scoped static
两单元同步注释/台账，重审不增加103完成数；其它缓存/统计和原型发布问题仍各自核查。
这里只用完整静态可达链标记，未执行并发复现、不修改行为；后续建立明确归属或完整同步。

源码证明与测试缺口见对应台账evidence；数字引用一并迁移，旧211无关checker问题身份
保留。编译沿用实际r120基线，对照一次真实TU；不授完整模块、全表或跨仓捕获信用。
