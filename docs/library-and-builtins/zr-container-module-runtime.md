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
doc_type: module-detail
---

# zr.container 原生模块与反射注册契约

本页描述 module.c 当前静态调用链；不宣称本轮运行测试、GC、OOM、ABI或性能通过。
已审批注释候选覆盖本文件161函数及全部状态、字段、枚举、注册项与非显然块，
357单元台账不等于相关core/library/parser依赖全文件已审。

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
