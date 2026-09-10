---
related_code:
  - scripts/validate_wiki.py
  - docs/wiki/manifest.json
  - zensical.toml
  - requirements-docs.txt
  - .github/workflows/wiki-pages.yml
  - docs/wiki/stylesheets/extra.css
  - docs/pygments_zr/zr_pygments/lexer.py
implementation_files:
  - scripts/validate_wiki.py
  - tests/scripts/test_validate_wiki.py
  - tests/scripts/test_wiki_theme_assets.py
  - tests/scripts/test_zr_pygments.py
  - .github/workflows/wiki-pages.yml
plan_sources:
  - user: 2026-09-10 继续细化 Wiki，后续用于搭建 Wiki 网页
  - docs/plans/syntax/README.md
  - docs/library-and-builtins/index.md
tests:
  - tests/scripts/test_validate_wiki.py
  - tests/scripts/test_wiki_theme_assets.py
  - tests/scripts/test_zr_pygments.py
doc_type: workflow-detail
---

# Wiki 编写、索引与发布规范

本页是维护 `docs/wiki` 的操作手册。它把每篇说明书如何组织、如何声明源码证据、如何
加入机器索引、如何验证本地链接以及如何发布到 GitHub Pages 固定下来。网页生成器可以
替换，但这些 source contract 仍是 Wiki 的输入协议。

## 1. 信息架构

Wiki 使用“顶层章节 + 领域 leaf page + API/实现专题”的三层结构：

```text
docs/wiki/
  README.md                 # 人类入口，不加入 pages manifest
  01-overview.md ...        # 顶层章节页
  02-language/              # 语言手册和语法专题
  03-modules/               # 官方 provider 和 API leaf
  04-tools/                 # CLI、LSP、测试、Wiki 构建
  05-interop/               # C/Rust/AOT/插件接口
  06-reference/             # artifact、错误、术语、状态矩阵
  manifest.json             # 网页路由和父子关系
```

顶层编号表达推荐阅读顺序，不表达 ABI 版本。新增页面应归入已有目录；只有出现独立
导航、独立验证和稳定边界时才增加新章节。一个页面只描述一个主要 contract：例如
`native-plugin-loading.md` 负责发现/加载/失效，descriptor 字段细节留给
`native-module-authoring.md`，调用绑定留给 `native-registry-call-binding.md`。

## 2. 必需 front matter

每个 Markdown（包括各目录的 `index.md`，不包括根 `README.md` 的 manifest 条目）必须
以 YAML front matter 开始，并包含以下五个 key：

```yaml
---
related_code:
  - public/header/or/design/file.h
implementation_files:
  - src/implementation/file.c
plan_sources:
  - docs/plans/accepted-design.md
tests:
  - tests/feature/test_feature.c
doc_type: api-reference
---
```

字段的语义如下：

| 字段 | 应写什么 | 不应写什么 |
| --- | --- | --- |
| `related_code` | 读者需要理解 contract 的公共头、设计文档或 fixture | 随意列整个仓库、与页面无关的文件 |
| `implementation_files` | 实际实现该行为的源码 | 只用于猜测的旧文件 |
| `plan_sources` | 用户需求、已接受设计、路线图或本页依据 | 把 planned 目标写成 current 证据 |
| `tests` | 能验证页面结论的测试/acceptance 记录 | 从未运行或不存在的测试路径 |
| `doc_type` | `guide`、`api-reference`、`module-detail` 等稳定类别 | 把状态 (`current`) 塞进类型名 |

validator 目前只检查 key 是否存在，不解析 YAML 值；但网页生成和人工审阅依赖路径真实
存在。路径应使用仓库相对、正斜杠和 ASCII 文件名。若行为只在某配置可用，应在正文标明
`experimental`/`planned`，不要从 front matter 推断可用性。

## 3. 页面内容模板

面向用户的 API/功能页建议按下面顺序写，读者可以从用例一路追到 C 接口：

1. **状态和定位**：模块名、provider phase/tier、适用读者、非目标边界。
2. **最小可运行用例**：导入、构造、成功结果和必要的 close/await。
3. **语法或签名**：EBNF、源级签名、参数 passing mode、返回值和默认值。
4. **行为规则**：成功、空值、异常、边界、复杂度和线程/ownership 限制。
5. **实现机制**：parser AST、canonical type、SemIR/CFG、descriptor、runtime/cache 的
   实际连接；明确哪些阶段尚未检查。
6. **C/Rust 接口**：准确的头文件、参数、owner、错误码和可复制调用顺序。
7. **失败矩阵和排错**：按 parser -> semantic -> compiler -> loader -> runtime 排列。
8. **证据链接**：公共头、实现文件、测试和相关专题页。

不要只把函数名罗列成“API 清单”。每个导出至少说明输入 shape、结果 ownership、失败
方式和一个正例；资源型接口还要说明 close 幂等性和跨 `await`/thread/reload 的规则。

## 4. 语法和代码块约定

| fence | 用途 | 约束 |
| --- | --- | --- |
| `zr` | ZR 源码 | 使用当前 parser 实际接受的语法；删除语法要标 migration |
| `ebnf` | grammar 摘要 | `[]` 可选、`{}` 重复，不能把花括号误写成源码示例 |
| `c` | C host/native callback | API 名称和参数必须来自当前 public header |
| `text` | 状态机、artifact 布局、诊断输出 | 不要混入可复制源码 |
| `json` | manifest/artifact 示例 | 字段保持可解析，省略字段要明确说明 |

ZR 代码应优先展示完整上下文：`import`、变量类型、错误分支、resource close 和返回值
不要省略到读者无法判断 owner。C 代码中的 `ZR_NULL` 是指针 sentinel；文中 ZR 的
`null` value 必须使用不同表述。不会编译的伪代码请标注“示意”，并避免使用看起来像
真实公共函数的占位名称。

代码 fence 外的链接才会被 validator 检查；不要把未完成的 URL 放进 fence 外文本。标题
应有唯一、可读的 slug；链接到标题时使用生成的 lower-case anchor，不要依赖手写 HTML id。

## 5. manifest 注册

新增 Markdown 后必须在 `docs/wiki/manifest.json` 的 `pages` 数组加入一条记录：

```json
{
  "id": "interop-native-plugin-loading",
  "path": "05-interop/native-plugin-loading.md",
  "kind": "guide",
  "parent": "interop"
}
```

规则：

- `id` 全局唯一且保持稳定；改标题不应随意改 id。
- `path` 相对于 `docs/wiki`，只能是 `.md`，不得包含 `..` 或绝对路径。
- `parent` 必须是 `sections` 中的 id；顶层页使用 `null`。
- `kind` 应与页面用途一致：`guide`、`api`、`module`、`reference`、`tool` 等。
- `README.md` 作为人类入口不放进 `pages`；所有其它 Markdown 必须恰好出现一次。

如果新页面代表新顶层导航，同时在 `sections` 增加 `{id,path,kind}`，并在相应目录的
`index.md`、根 README 的阅读路径和页面地图中加入链接。manifest 是网页路由的机器真相，
目录文件名和手写表格不能替代它。

## 6. 本地链接和锚点

链接相对于当前 Markdown 文件解析：

```markdown
[声明参考](../02-language/declaration-statement-reference.md)
[本页的错误](#错误矩阵)
[插件加载](../05-interop/native-plugin-loading.md#加载状态机)
```

validator 会检查目标文件存在、目标是 Markdown 时 anchor 存在，并忽略 `http:`, `https:`,
`mailto:`, `tel:`, `data:` 和根绝对路径。重命名 heading 会改变 slug，应同步修正所有
链接；重复 heading 会产生 `-1`、`-2` 后缀，不要依赖不稳定的重复标题。外部 GitHub/Pages
链接只用于仓库/发布说明，仓内页面优先使用相对链接，保证离线预览可用。

## 7. 源码证据和状态标签

写“当前行为”时采用以下证据优先级：

1. public `include/` 头文件中的签名、枚举和结构体；
2. production parser/compiler/runtime 的分支；
3. 正/负 fixture、单元测试和 acceptance 记录；
4. 已接受设计文档；
5. 旧 Wiki、注释或路线图。

当证据冲突时，正文应明确写出限制，例如“parser 已接受，semantic 尚未验证”或
“仅 CompileTool phase 可用”。`planned` 功能不能放进“稳定导出”表；`experimental` 功能
应同时列出关闭条件、缺失测试或 ABI 风险。错误码、magic、ABI 数值、文件后缀等可复制
常量要给出来源文件，避免文档生成第二套不一致的真相。

## 8. 运行验证

在提交文档前从仓库根目录运行：

```bash
python scripts/validate_wiki.py --root .
python -m unittest tests/scripts/test_validate_wiki.py \
  tests/scripts/test_wiki_theme_assets.py \
  tests/scripts/test_zr_pygments.py -v
python -m pip install -r requirements-docs.txt
zensical build --clean --strict
python scripts/validate_wiki.py --root . --site-dir site
```

第一个脚本检查 front matter、fence、manifest、页面覆盖和本地链接；单元测试覆盖故障
输入；strict build 检查 Zensical 的导航、模板和未知链接；最后一步确认 `site/index.html`
真实存在。`site/` 是生成目录，已加入 `.gitignore`，不要把它加入提交。

如果只改了页面文字，也应至少运行 source validator；如果增加了 fence、heading、manifest
或主题资源，运行完整四步。CI 使用同一套命令，另会确认 `zr` Pygments lexer 已注册，并
检查生成的 `03-language-syntax/index.html` 含 `language-zr` 和关键字 token class。

## 9. 主题和前端约束

`zensical.toml` 定义站点名称、目录 URL、导航/search 功能、系统/浅色/深色 palette 和
`stylesheets/extra.css`。主题 CSS 只负责 ZR token、代码边框和 reduced-motion；正文颜色
应使用主题变量，不能在页面内写死白底/黑字或添加与内容无关的装饰。新增样式前先检查
`tests/scripts/test_wiki_theme_assets.py` 的契约；不要把生成 HTML 或截图作为 source。

代码块应使用 `zr`/`zro`/`zrs`/`zrp` alias，让自定义 lexer 给关键字、类型、字符串、数字、
函数和运算符分色。未知 fence 会退化为纯文本但不应被当作高亮成功。代码示例太长时拆成
“最小用例 + 完整宿主流程”，避免移动端横向滚动遮挡关键参数。

## 10. Pull request 检查表

- 新文件有五个必需 front matter 字段，且列出的源码/测试路径真实存在。
- 新文件已加入 manifest，id/path/parent 唯一且正确。
- 目录 index、根 README 的阅读路径和页面地图有入口链接。
- 所有代码示例使用当前语法/API；旧语法明确标注 migration。
- 每个功能写了成功、失败、ownership、phase 和实现机制，而不只是名称。
- C API 示例说明 include、返回值、错误读取和释放顺序。
- `validate_wiki.py`、三个脚本测试和 strict build 全部通过。
- 未把 `site/`、临时日志、机器本地路径或未验证的未来设计加入提交。

## 11. 发布与回滚

`.github/workflows/wiki-pages.yml` 在 `main` push 和面向 `main` 的 pull request 上运行构建；
只有 `main` push 或从 `main` 手动 `workflow_dispatch` 才部署 Pages。发布失败时先查看
source validator 的文件/行号，再看 Zensical strict 输出；不要通过关闭 strict 或删除链接
来绕过门禁。Pages artifact 是构建产物，不是 source of truth，回滚应回滚 Markdown、
manifest 或 workflow 的 Git 变更，而不是手工替换 `site/`。

## 12. 变更说明模板

为便于后续网页生成和审阅，提交说明可使用以下最小信息：

```text
Docs: add <page-id>
- scope: syntax / module / C API / tool
- evidence: headers, implementation files, tests
- validation: validate_wiki, unittest, zensical strict
- status: current / experimental / planned / migration
```

这份模板不是额外机器协议；它帮助审阅者确认页面是否真的覆盖用户要求的语法、用例、
实现机制和宿主接口。功能变更时，应先改源码和测试，再同步本页 front matter 与相关
专题页，保持“代码 -> 测试 -> Wiki -> 网页”的顺序可追溯。
