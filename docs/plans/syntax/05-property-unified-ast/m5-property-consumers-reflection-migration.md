# 05-M5 Property Consumers/Reflection/Migration 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-05-property-unified-ast-design.md` 的
`M5 LSP/reflection/migration`，执行清单：
`docs/plans/syntax/05-property-unified-ast/m5-property-consumers-reflection-migration-implementation-plan.md`。

## 状态与产出记录

- 完成时间：2026-07-24 08:16 +08:00
- 状态：completed
- 完成项目：
  - 已审计当前 reflection 与 LSP 仍存在 `__get_`/`__set_`、`ZR_AST_CLASS_PROPERTY` 配对路径；
    将总设计拆成 canonical PropertyQuery、固定宽 PropertyDef、identity-driven reflection、LSP
    source/binary parity、structured legacy migration 和三工具链晋级六个顺序任务。
  - 已完成 Task 1 分层 RED：lower-layer 明确缺少 PropertyQuery 与 PropertyDef 40/44 字节
    载荷；LSP interface 的 unified property/migration/navigation/hover 合同保持 RED；stdio
    hover 对 property leading documentation 的断言以真实进程 `exit 1` 固定。各层证据分别保留，
    未把协议层失败归入 parser/query 层修复。
  - 已发布 owned `propertyContracts` 与 `PropertyAt`/`PropertyBySymbolId`，由
    `compiler_property.c` 直接写入 PropertySymbol/accessor SymbolId、canonical TypeId、access、
    receiver/ref/range 合同；真实 source compile/query/reset 断言 GREEN。PropertyDef 保持 48
    字节并在 offset 40/44 发布 initializer token/name offset，artifact schema 14/14 GREEN；
    reflection focused 仍只保留缺少 canonical getter link 的预期 RED，未提前计入 Task 3。
  - Task 2 lower-layer gate 已完成：M1/M2/M3 property targets、M4 23/23、canonical
    consumers 16/16、semantic query 27/27、artifact schema 14/14 均以真实进程 `exit 0`
    通过；M4 必须从仓库根启动其内部 AOT C 编译，build-dir cwd 的缺 include 运行已作废。
  - Task 3 已将 current property reflection 从 4350 行 `reflection.c` 拆到独立
    `reflection_property.c`，以 visible carrier + `propertyIdentity` + structured accessor role
    关联 getter/setter/init；source 与 `.zro` reload、普通 `__get_fake` 方法负边界、access/
    receiver/ref-export、compile-time decorator 及 AOT reflection annotation 均 GREEN。module
    runtime descriptor 的 canonical property gate 已通过；完整 module-system、reflection-token
    与 metadata-token 的后续 GC/union 失败已在 clean `5ef5d9b` 重放为完全一致的既有基线，
    未以 M5 兼容代码掩盖。
  - Task 4 source consumer 已完成：LSP 仅调用 bodyless canonical property binder，统一 property
    symbol 保存 PropertySymbol/TypeId、accessor/value parameter SymbolId 与 selection range；hover/
    completion/definition/prepareRename、隐藏 accessor 负边界和 contextual `value` parameter token
    同时 GREEN。parser property consumer 4/4、interface source focused 真实进程 `exit 0`；binary
    分项 RED 进一步固定为 imported `Meter` prototype `members=0`，hover label、visible completion、
    binary definition 同时 unavailable，未在 LSP 按 `shared`/`__get_shared` 文本补偿，Task 4 Step 4/6
    继续保持未完成。
  - Task 4 binary support-first 链已由 RED 收口：binary importer 仅把 exact compiled prototype rows
    合并进同名空 imported placeholder，并从 `propertyIdentity/accessorRole/type/ref` 结构化字段发布
    PropertyQuery/PropertyAt；imported type stub 保存 canonical module provenance。LSP 再按
    PropertySymbolId join visible row，hover、唯一 `shared` completion、隐藏 getter 负边界和 binary
    module definition 全部 GREEN。当前 `.zro` v34 不含 nested PropertyDef coordinate table；依据
    Syntax 01 M5 已冻结的 `SZrIo` compatibility 边界，Task 4 Step 4 明确以 exact compiled carrier
    作为当前 executable bridge，而不是伪造不存在的 PropertyDef token join。新增 lower-layer 断言
    验证 visible/getter/setter 共享 property identity、TypeId/ref 位来自专用槽、普通 `__get_*` 保持
    method role，且 source→write→reload 的整段 prototype bytes 完全一致；9/9 GREEN。canonical
    PropertyDef 固定宽/token schema 仍由 Task 2 验证，正式 `ZRAF` executable cutover 留在既有后继边界。
  - Task 5 已完成 structured migration 与 canonical refactor action：parser 以稳定
    `legacy_property_syntax` diagnostic 发布 declaration/name/type/body related ranges，只对单 accessor
    或相邻且 owner/name/type/static/access 完全匹配的 getter/setter 生成完整 unified-property edit；
    LSP 只消费 structured fix。缺 interface set/init 由 exact PropertySymbol 查询与 transitive
    interface contract 决定，explicit-field proxy 为 field/property 分配独立 SymbolId。二义 interface、
    ref property、binary-only declaration、invalid fallback snapshot 与不安全 legacy pair 均无 action。
    parser/property focused 7/7、GCC LSP interface 完整进程真实 `exit 0`；新增 canonical query、
    binary provenance 与 stale/invalid 负边界全部 PASS。大文件拆分后 `parser_property.c` 293 行、
    `parser_property_migration.c` 761 行，未继续扩大原有 parser 单文件。
  - Task 6 stress/incremental gate 已完成：lower-layer 生成并绑定 128 个 visible property 与
    256 个 accessor，逐项验证 property/getter/setter SymbolId 唯一且链接完整；深层 interface
    requirement 通过 transitive contract 查询。LSP 在 64-property 文档上证明 body-only edit 保持
    edited/unrelated PropertySymbolId 与 TypeId，单 property 类型变更只替换对应 TypeId，unrelated
    property identity 保持稳定，更新后的 hover 为 canonical `property p032: string`。metadata
    stripping RED 证明 getter/setter 曾被错误裁剪；修复后 AOT reachability 只从 compiled row 的
    `propertyIdentity/accessorRole/functionConstantIndex` 发布 `root.property_accessor`，保留两个
    accessor 并继续裁剪普通未使用 method。focused property 9/9、既有 AOT stripping 10/10 与
    GCC interface 完整进程均为真实 `exit 0`。
  - Task 4/6 最终 LSP gate 已在同一冻结 overlay 上收口：GCC、Clang、MSVC 的 interface、project、
    UTF-16、source-contract 与 local semantic query 目标均真实 `exit 0`，source/binary property
    hover、唯一 completion、definition、rename、contextual `value`、migration/refactor 与 incremental
    identity 合同均由 canonical PropertyQuery/PropertySymbol 驱动。MSVC 首轮 interface 暴露
    `typePrototypes` 在 native lazy import 的 `realloc` 后继续解引用旧元素的 heap-use-after-free；
    ASan 精确定位后，receiver consumer 在递归查询前捕获稳定 name/import-module/type/imported
    facts，修复后 MSVC ASan 3/3、普通 Debug 10/10 连续完整运行通过，未增加 property-name fallback。
  - 冻结三工具链矩阵完成：property M1-M5 分项分别为 16/16、21/21、22/22、23/23、9/9，
    semantic query 27/27、artifact schema 14/14，AOT stripping/annotation 为 10/10、12/12、3/3；
    GCC/Clang/MSVC 全部对应进程真实 `exit 0`。三套 stdio/CLI smoke 均真实 `exit 0`，CLI 输出
    精确为 `40`。并发使用同一 fixture 的一轮 GCC/Clang 结果因生成文件竞争整轮作废，最终证据
    只采用顺序重放日志。
  - clean `5ef5d9b` 已证明 module-system、metadata-token、reflection-token 与 local-semantic-hover
    的非零/Debug 等待在 M5 前即存在；最终 overlay 逐项保持同一失败形态，因此这些目标明确记录
    为既有基线且不宣称 GREEN。exact ownership 共 70 paths，冻结 snapshot 与工作树逐文件
    SHA-256 一致，`git diff --check` 通过，三份外部 Syntax 草案、生成目录与其他 forbidden 路径为
    0，提交前共享 index 为空。
  - M5.1 property variance follow-up 已完成：interface variance responsibility 已从 2708 行的
    `semantic_analyzer_typecheck.c` 提取到 `semantic_analyzer_variance.c`，并经窄内部接口调用。
    LSP semantic analyzer 现与 compiler canonical variance validator 一致地消费统一
    `ZR_AST_PROPERTY_DECLARATION` 的 structured accessor kinds。仅 `get` 为协变、仅 `set/init`
    为逆变、混合访问器为不变；没有 property name 或 source text fallback。回归 fixture 同步从已废弃的 `pub get/set` declaration 迁移为
    `property item: T { get/set; }`，仍严格要求六个 `invalid_variance` diagnostics。GCC、
    Clang、MSVC 的 LSP 18-target matrix 均为真实 exit 0，三套 main/position/diagnostic-fix
    stdio/CLI smoke 均 exit 0。native constructor/foreach/container 的五个既有 Unity marker
    与此叶无关，未作为通过证据；详情见 M5.1 completion record。

## 当前实现边界

- M1-M4 已提供统一 PropertyDecl/PropertySymbol、显式 field/init、typed get/set/init、ref Place/
  LoanId、managed reference、artifact/VM/AOT；M5 只能消费这些 canonical facts。
- 现有 hidden accessor 名称仍可作为旧 executable artifact 的窄兼容 reader，但不得继续作为
  current source、current artifact、reflection 或 LSP 的语义来源。
- 三份外部 dirty Syntax 草案、其他 LSP 计划/状态文档和生成目录不属于本里程碑。

## 验收结论

- 已完成。PropertyQuery/PropertyDef、source/binary reflection、LSP canonical consumer、structured
  migration、stress/incremental、AOT stripping 与同一冻结快照三工具链门禁均通过；既有基线失败
  已由 clean HEAD 对照隔离，未以兼容逻辑或放宽断言掩盖。
