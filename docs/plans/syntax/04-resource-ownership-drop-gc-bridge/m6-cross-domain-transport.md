# 04-M6 cross-domain transport 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md`
的 `M6 跨 domain transport`。

## 状态与产出记录

- 完成时间：2026-07-22 19:19 +08:00
- 状态：已完成
- 已完成项目：
  - 已把artifact与TypeLayout schema升级到v2，新增按TypeDef token唯一排序的固定宽度
    `DOMAIN_TRANSFER_TABLE`，结构化发布五种transfer kind、schema identity、provider identity
    与flags；VM与AOT C TypeLayout消费同一合同。
  - 已让显式`Forbidden`与缺省合同可区分；仅GcFree blittable POD可缺省为`ValueCopy`，
    provider-backed layout含GC/ownership/ref字段时拒绝，避免隐藏source-domain edge。
  - 已在opaque TransferEnvelope中实现cross-domain source/target identity、generation、kind、
    schema/provider identity、quota、serialized payload与structured diagnostic。
  - 已实现scalar与closed GcFree inline `ValueCopy`；prepare快照源值，commit按target layout
    version/hash校验，不从当前source storage重读。
  - 已实现保留alias/cycle的`StructuredClone`，拒绝foreign-domain edge并执行object/byte/depth
    quota；cycle backedge先按object identity复用，避免把回边误报为depth overflow；target
    decode期间所有已分配对象与临时GC value均由domain root handle保护。
  - 已实现provider-backed `ImmutableHandle`与`ResourceMove`；prepare/commit返回structured
    status，zero/partial token失败也调用no-throw abort，provider成功但未构造target视为失败；
    provider descriptor按值快照，commit/abort callback在envelope锁外执行，partial target失败先
    释放target owner再保留token供单次abort。
  - 已固定`ResourceMove`的DropOnFailure合同：success、target allocation/decode/provider failure、
    cancellation与domain shutdown均只有一个terminal owner和一个cleanup path，不恢复source Place。
  - 已用release/acquire直接竞态测试覆盖publish/abort、claim/abort、commit/abort、immutable
    multi-target与高频producer/consumer litmus；stale generation/worker/epoch和duplicate completion
    均拒绝。
  - 已由独立review复核并关闭clone root、callback锁重入、partial target cleanup、provider
    descriptor lifetime、ValueCopy stale status与artifact optional-section边界；最终复审无剩余
    Critical/Important finding。

## 最终验证结果

- 2026-07-22 17:24 +08:00的首轮矩阵仅用于发现问题；随后独立review驱动root/lifetime/
  cleanup/status修复、focused RED/GREEN与完整矩阵重放，首轮结果未计入最终晋级。
- 最终固定source snapshot为`HEAD b2edae30102f171f0c999db2a99ee1819d75f61e + 31个M6
  code/test/docs exact overlays`；明确排除3份既有dirty Syntax草案、LSP路径、`exception.c`、
  根目录生成物和全部build/log目录。
- GCC 11.4与Clang 14.0在独立WSL目录、MSVC 14.44.35207在独立Windows Ninja目录
  完成同一20-target矩阵；三套分别20/20真实进程`exit 0`，Unity合计各506/506、0 failure。
- focused M6 suites在三工具链分别为：TypeLayout 9/9、artifact 14/14、cross-domain
  functional 23/23、cross-domain race 5/5；same-domain handoff 7/7及既有domain/ownership/
  GC/parser/compiler/AOT消费者同步保持GREEN。
- Clang ASan+UBSan fresh build下TypeLayout 9/9、artifact 14/14、cross-domain functional
  23/23、race 5/5，共51/51通过且无sanitizer report。WSL LeakSanitizer在并发启动多个
  instrumented进程时出现一次无报告core dump，TypeLayout随后串行开启leak detection仍9/9；
  其余三项按既有WSL边界使用`detect_leaks=0`并保持ASan/UBSan开启。
- Clang ThreadSanitizer在WSL中使用`setarch x86_64 -R`解决runtime address mapping限制后，
  直接运行5/5并发线性化/litmus测试，0 data-race report。Helgrind因不能识别本项目GCC
  `__atomic_*`原子而产生19组工具误报，未作为clean证据。
- TDD/故障矩阵包含：五kind artifact roundtrip与非法provider/duplicate row；inline ValueCopy
  snapshot/schema mismatch；clone alias/cycle、object/byte/depth quota、foreign edge、decode
  failure；ResourceMove provider prepare/commit、target allocation、decode、success-without-target、
  source/target shutdown abort与stale generation；provider descriptor snapshot、锁外callback
  snapshot重入、partial target失败释放、clone decode root presence、partial token abort及每条
  路径exactly-one cleanup。

## 当前实现边界

- M6只提供runtime/artifact transport，不接入`zr.thread`/TaskScheduler，不发布语言层
  `Send/Sync`，也不承诺消息送达或Job副作用exactly-once。
- Provider callback可进入GcAware native scope和poll，但不得递归驱动同一个provider token或
  envelope；runtime按值快照provider descriptor，但provider-owned `userData`必须保活到唯一
  terminal `Free`返回；runtime不按callback地址/native function name推断行为。
- Envelope由单个terminal disposer释放；当前API不支持多个线程并发调用Free。
- Syntax12 scheduler只能在本gate完成后消费此transport，并须保留transfer id、claim epoch与
  DropOnFailure合同，不能回退到legacy dynamic transport。
