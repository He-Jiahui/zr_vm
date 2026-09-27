import * as vscode from 'vscode';
import { onDidChangeLanguageClient, sendLanguageServerRequest } from './languageClientRequests';
import {
    normalizeRichHoverSections,
    renderRichHoverHtml,
    summarizeRichHover,
    type RichHoverPayload,
    type RichHoverRenderModel,
    type RichHoverSection,
} from './richHoverShared';

/** 与 package.json 的侧栏声明一致，供扩展宿主定位 Rich Hover 视图。 */
export const ZR_RICH_HOVER_VIEW_ID = 'zrRichHover';
/** 普通 Hover 的命令链接与命令面板共用此入口，将当前符号固定到侧栏。 */
export const ZR_RICH_HOVER_FOCUS_COMMAND = 'zr.richHover.focus';
/** 侧栏标题按钮和命令面板共用此入口，重新查询当前目标。 */
export const ZR_RICH_HOVER_REFRESH_COMMAND = 'zr.richHover.refresh';

// 桌面 stdio 与 Web Worker 都提供此扩展请求；旧服务端缺失方法时请求层返回无结果。
const ZR_RICH_HOVER_REQUEST = 'zr/richHover';
const ZR_WORKBENCH_VIEW_COMMAND = 'workbench.view.extension.zr';
const REFRESH_DEBOUNCE_MS = 150;
const SUPPORTED_LANGUAGE_ID = 'zr';
const SUPPORTED_EXTENSION = '.zrp';
const COMMAND_MARKER = 'command:zr.richHover';

/** 跨 JSON-RPC 边界的零基位置；服务端 range 与 VS Code Range 在此转换。 */
type SerializedPosition = {
    line: number;
    character: number;
};

/** 与 Rich Hover 响应一起返回的可选符号范围。 */
type SerializedRange = {
    start: SerializedPosition;
    end: SerializedPosition;
};

/** 普通 Hover、侧栏和命令 URI 共享的目标快照，不能依赖后续仍有活动编辑器。 */
type RichHoverTarget = {
    uri: vscode.Uri;
    line: number;
    character: number;
};

/** 扩展请求在 sections 之外可携带普通 Hover 使用的符号范围。 */
type RichHoverPayloadWithRange = RichHoverPayload & {
    range?: SerializedRange;
};

/** 命令 URI 序列化目标；无完整位置时 focus 保留当前显式目标。 */
type RichHoverCommandArgs = {
    uri?: string;
    line?: number;
    character?: number;
    // TODO: 当前仓内只由 buildCommandUri 生成前三项，focus 未消费 preserveFocus；核查外部命令调用是否需要保持编辑器焦点。
    preserveFocus?: boolean;
};

/** 两种语言客户端共用的 Hover 中间件形状，next 保留标准 LSP Hover 回退。 */
type HoverMiddleware = {
    provideHover?: (
        document: vscode.TextDocument,
        position: vscode.Position,
        token: vscode.CancellationToken,
        next: (
            document: vscode.TextDocument,
            position: vscode.Position,
            token: vscode.CancellationToken,
        ) => vscode.ProviderResult<vscode.Hover>,
    ) => vscode.ProviderResult<vscode.Hover>;
};

/** 宿主在激活时注册视图，在每次客户端启动时接入 Hover，在停用时释放订阅。 */
export interface ZrRichHoverController extends vscode.Disposable {
    createMiddleware(): HoverMiddleware;
    refresh(): Promise<void>;
}

// 共享纯函数也供节点测试导入；桌面和 Web 的运行时仍走同一个实现。
export { renderRichHoverHtml, summarizeRichHover, type RichHoverSection } from './richHoverShared';

/** 在语言客户端启动前注册命令和侧栏，使重启期间仍保留视图入口并等待新客户端。 */
export function registerRichHoverSupport(context: vscode.ExtensionContext): ZrRichHoverController {
    return new ZrRichHoverService(context);
}

/** 维持悬停摘要与侧栏详情的同一目标语义，跨客户端重启保留视图实例。 */
class ZrRichHoverService implements ZrRichHoverController, vscode.WebviewViewProvider {
    // 宿主级监听与命令统一持有；客户端重启不应注销它们。
    private readonly disposables: vscode.Disposable[] = [];
    // 视图可能晚于控制器创建，也可能在生命周期内未被打开。
    private webviewView: vscode.WebviewView | undefined;
    // 串行化显式与事件触发的刷新，避免较早的异步查询最后覆盖新目标。
    private refreshChain: Promise<void> = Promise.resolve();
    // 合并编辑器高频事件；dispose 必须取消尚未执行的刷新。
    private refreshTimer: ReturnType<typeof setTimeout> | undefined;
    // 命令链接携带 Hover 的位置；用户切换编辑器或光标后回到活动编辑器。
    private explicitTarget: RichHoverTarget | undefined;
    // Webview 未创建时先缓存模型，创建后才写入 HTML。
    private currentModel: RichHoverRenderModel = emptyRenderModel();

    /** 注册宿主级入口与编辑器事件；这些订阅随扩展实例释放，而非随 LSP 会话重建。 */
    constructor(private readonly context: vscode.ExtensionContext) {
        this.disposables.push(
            vscode.window.registerWebviewViewProvider(ZR_RICH_HOVER_VIEW_ID, this, {
                webviewOptions: {
                    retainContextWhenHidden: true,
                },
            }),
            vscode.commands.registerCommand(ZR_RICH_HOVER_FOCUS_COMMAND, async (args?: RichHoverCommandArgs) => {
                await this.focus(args);
            }),
            vscode.commands.registerCommand(ZR_RICH_HOVER_REFRESH_COMMAND, async () => {
                await this.refresh();
            }),
            vscode.window.onDidChangeActiveTextEditor(() => {
                this.explicitTarget = undefined;
                this.scheduleRefresh();
            }),
            vscode.window.onDidChangeTextEditorSelection((event) => {
                if (event.textEditor === vscode.window.activeTextEditor) {
                    this.explicitTarget = undefined;
                    this.scheduleRefresh();
                }
            }),
            vscode.workspace.onDidChangeTextDocument((event) => {
                // TODO: 编辑不会清除显式目标；若文档在目标前插入文本且未触发光标事件，
                // 侧栏仍查询旧坐标。核查 VS Code 事件顺序及定位目标是否需随编辑重映射。
                if (vscode.window.activeTextEditor?.document.uri.toString() === event.document.uri.toString()) {
                    this.scheduleRefresh();
                }
            }),
            vscode.workspace.onDidOpenTextDocument((document) => {
                if (isSupportedDocument(document)) {
                    this.scheduleRefresh();
                }
            }),
            vscode.workspace.onDidCloseTextDocument((document) => {
                if (this.explicitTarget?.uri.toString() === document.uri.toString()) {
                    this.explicitTarget = undefined;
                }
                this.scheduleRefresh();
            }),
            // 客户端替换不会重建侧栏，因此必须在新会话发布后重新查询。
            onDidChangeLanguageClient(() => {
                this.scheduleRefresh();
            }),
        );

        // TODO: 宿主同时保存这些句柄和控制器自身，deactivate 还会调用 dispose；
        // 核查 VS Code 各类 Disposable 的重复注销约定及统一所有权是否必要。
        context.subscriptions.push(...this.disposables);
        void this.refresh();
    }

    /** VS Code 首次创建侧栏时呈现最新模型；只读 HTML 不需要 Webview 脚本。 */
    resolveWebviewView(webviewView: vscode.WebviewView): void | Thenable<void> {
        this.webviewView = webviewView;
        webviewView.webview.options = {
            enableScripts: false,
        };
        this.renderCurrentModel();
    }

    /**
     * 给桌面与 Web 客户端的标准 Hover 加上结构化摘要和侧栏链接。
     * 自定义请求无结果时调用 next，保持旧服务端的普通 Hover 能力。
     */
    createMiddleware(): HoverMiddleware {
        return {
            provideHover: async (document, position, token, next) => {
                const target = toTarget(document, position);
                // BUG: Web Worker 文档快照过期时返回 ContentModified，请求层会重抛，
                // 此 await 使标准 Hover 的 next 也无法执行；应核查是否按可选能力降级。
                const payload = await fetchRichHoverPayload(target);

                if (token.isCancellationRequested) {
                    return next(document, position, token);
                }

                // 仅有效结构化节能替代普通 Hover；否则保留 next 的结果并附入口。
                if (payload && normalizeRichHoverSections(payload.sections ?? []).length > 0) {
                    return buildSummaryHover(target, payload);
                }

                const baseHover = await Promise.resolve(next(document, position, token));
                if (token.isCancellationRequested) {
                    return baseHover;
                }

                return appendCommandLinkToHover(baseHover, target);
            },
        };
    }

    /** 按事件到达顺序刷新侧栏；失败不会永久阻塞后续入队的刷新。 */
    async refresh(): Promise<void> {
        // BUG: loadRenderModel 的非兼容性请求错误会使本次 Promise 拒绝；构造与定时器
        // 以 void refresh 启动且无 catch，导致未处理拒绝并保留旧侧栏内容。
        this.refreshChain = this.refreshChain.then(
            async () => {
                this.currentModel = await this.loadRenderModel();
                this.renderCurrentModel();
            },
            async () => {
                this.currentModel = await this.loadRenderModel();
                this.renderCurrentModel();
            },
        );
        await this.refreshChain;
    }

    /** 注销宿主事件与命令；已发出的查询只会更新内存模型，不再写入已释放视图。 */
    dispose(): void {
        if (this.refreshTimer !== undefined) {
            clearTimeout(this.refreshTimer);
            this.refreshTimer = undefined;
        }

        this.webviewView = undefined;
        for (const disposable of this.disposables) {
            disposable.dispose();
        }
        this.disposables.length = 0;
    }

    /** 打开侧栏并优先展示命令 URI 指向的符号；无参数时沿用当前目标。 */
    private async focus(args?: RichHoverCommandArgs): Promise<void> {
        // TODO: 用户从命令面板无参打开时可能仍保留旧的显式 Hover 位置；
        // 核查是否应在无参数时改为跟随活动编辑器。
        const explicitTarget = parseCommandTarget(args);
        if (explicitTarget) {
            this.explicitTarget = explicitTarget;
        }

        await vscode.commands.executeCommand(ZR_WORKBENCH_VIEW_COMMAND);
        try {
            await vscode.commands.executeCommand(`${ZR_RICH_HOVER_VIEW_ID}.focus`);
        } catch {
            // VS Code may not expose the auto-generated focus command in every host.
        }

        await this.refresh();
    }

    /** 合并连续的光标和文档事件，避免每次输入都向客户端发送侧栏查询。 */
    private scheduleRefresh(): void {
        if (this.refreshTimer !== undefined) {
            clearTimeout(this.refreshTimer);
        }

        this.refreshTimer = setTimeout(() => {
            this.refreshTimer = undefined;
            void this.refresh();
        }, REFRESH_DEBOUNCE_MS);
    }

    /** 将自定义 sections 转为侧栏模型；旧服务端可通过标准 Hover 提供降级内容。 */
    private async loadRenderModel(): Promise<RichHoverRenderModel> {
        const target = this.resolveTarget();
        if (!target) {
            return emptyRenderModel();
        }

        const payload = await fetchRichHoverPayload(target);
        // TODO: 当前仅过滤空值，未校验 sections 是否为数组；核查跨版本响应
        // 的运行时形状及畸形响应能否安全回退到普通 Hover。
        const sections = normalizeRichHoverSections(payload?.sections ?? []);
        if (sections.length > 0) {
            const summary = summarizeRichHover(payload);
            return {
                title: summary.title,
                subtitle: describeTarget(target),
                sections,
            };
        }

        // 标准 Hover 包含本扩展添加的命令链接；降级文本必须在提取时去掉该链接。
        const fallbackText = await loadFallbackHoverText(target);
        if (fallbackText) {
            return {
                title: lastPathSegment(target.uri.path) || 'Rich Hover',
                subtitle: describeTarget(target),
                sections: [
                    {
                        role: 'docs',
                        label: 'Hover',
                        value: fallbackText,
                    },
                ],
            };
        }

        return {
            title: 'Rich Hover',
            subtitle: describeTarget(target),
            sections: [],
            status: 'No hover information is available for the current symbol.',
        };
    }

    /** 命令传入的精确位置优先；用户改变上下文后改用活动的 Zr 文档。 */
    private resolveTarget(): RichHoverTarget | undefined {
        if (this.explicitTarget) {
            return this.explicitTarget;
        }

        const editor = vscode.window.activeTextEditor;
        if (!editor || !isSupportedDocument(editor.document)) {
            return undefined;
        }

        return toTarget(editor.document, editor.selection.active);
    }

    /** 视图尚未创建时只保存模型，等 VS Code 调用 resolveWebviewView 再呈现。 */
    private renderCurrentModel(): void {
        if (!this.webviewView) {
            return;
        }

        this.webviewView.webview.html = renderRichHoverHtml(this.currentModel);
    }
}

/** 用少量语义字段维持编辑器 Hover 的可读性，完整 sections 留在侧栏。 */
function buildSummaryHover(target: RichHoverTarget, payload: RichHoverPayloadWithRange): vscode.Hover {
    const summary = summarizeRichHover(payload);
    const markdown = new vscode.MarkdownString();
    const commandUri = buildCommandUri(target);

    // 命令链接需要受信任的 Markdown；仅放行本视图命令，限制服务端文本的命令权限。
    markdown.isTrusted = {
        enabledCommands: [
            ZR_RICH_HOVER_FOCUS_COMMAND,
            ZR_RICH_HOVER_REFRESH_COMMAND,
        ],
    };
    markdown.supportThemeIcons = true;
    // TODO: summary.lines 来自服务端 Markdown 与源文档；核查受信任命令白名单下
    // 是否仍需转义源文本中的 Markdown 链接及特殊字符。
    markdown.appendMarkdown(summary.lines.join('\n\n'));
    markdown.appendMarkdown('\n\n---\n\n');
    markdown.appendMarkdown(`[Open rich panel](${commandUri})`);

    return new vscode.Hover(markdown, deserializeRange(payload.range));
}

/** 旧服务端无结构化结果时保留原 Hover 的内容和 range，仅增加侧栏入口。 */
function appendCommandLinkToHover(baseHover: vscode.Hover | null | undefined, target: RichHoverTarget): vscode.Hover | undefined {
    if (!baseHover) {
        return undefined;
    }

    const commandMarkdown = new vscode.MarkdownString();
    commandMarkdown.isTrusted = {
        enabledCommands: [
            ZR_RICH_HOVER_FOCUS_COMMAND,
            ZR_RICH_HOVER_REFRESH_COMMAND,
        ],
    };
    commandMarkdown.appendMarkdown(`\n\n---\n\n[Open rich panel](${buildCommandUri(target)})`);

    const contents = Array.isArray(baseHover.contents)
        ? [...baseHover.contents, commandMarkdown]
        : [baseHover.contents, commandMarkdown];

    return new vscode.Hover(contents, baseHover.range);
}

/** 经当前桌面或 Web 客户端请求扩展协议；可忽略的旧版本与重启错误视为无结果。 */
async function fetchRichHoverPayload(target: RichHoverTarget): Promise<RichHoverPayloadWithRange | undefined> {
    const result = await sendLanguageServerRequest<RichHoverPayloadWithRange | null>(ZR_RICH_HOVER_REQUEST, {
        textDocument: {
            uri: target.uri.toString(),
        },
        position: {
            line: target.line,
            character: target.character,
        },
    });

    if (!result || typeof result !== 'object') {
        return undefined;
    }

    return result;
}

/** 在侧栏查询标准 Hover，并剔除本扩展的导航链接以免侧栏显示自身入口。 */
async function loadFallbackHoverText(target: RichHoverTarget): Promise<string | undefined> {
    const hoverList = await vscode.commands.executeCommand<vscode.Hover[] | undefined>(
        'vscode.executeHoverProvider',
        target.uri,
        new vscode.Position(target.line, target.character),
    );

    if (!Array.isArray(hoverList) || hoverList.length === 0) {
        return undefined;
    }

    const fragments = hoverList
        .flatMap((hover) => normalizeHoverContents(hover.contents))
        .map(stripCommandLinks)
        .map((entry) => entry.trim())
        .filter((entry) => entry.length > 0);

    if (fragments.length === 0) {
        return undefined;
    }

    return fragments.join('\n\n');
}

/** 兼容 VS Code Hover 中的纯文本、Markdown 与代码片段，供只读侧栏统一展示。 */
function normalizeHoverContents(
    contents: vscode.Hover['contents'],
): string[] {
    const values = Array.isArray(contents) ? contents : [contents];
    const normalized: string[] = [];

    for (const value of values) {
        if (typeof value === 'string') {
            normalized.push(value);
            continue;
        }

        if (value instanceof vscode.MarkdownString) {
            normalized.push(value.value);
            continue;
        }

        if (typeof value === 'object' && value !== null && 'language' in value && 'value' in value) {
            normalized.push(String((value as { value?: unknown }).value ?? ''));
        }
    }

    return normalized;
}

/** 在 Hover 调用时捕获 URI 和位置，避免异步请求期间光标变化导致目标漂移。 */
function toTarget(document: vscode.TextDocument, position: vscode.Position): RichHoverTarget {
    return {
        uri: document.uri,
        line: position.line,
        character: position.character,
    };
}

/** 从链接命令还原 Hover 目标；参数缺失时不覆盖控制器已有目标。 */
function parseCommandTarget(args?: RichHoverCommandArgs): RichHoverTarget | undefined {
    // TODO: 公共命令接受外部参数，但这里只检查 number 类型；核查负数、NaN
    // 或非整数坐标是否应拒绝，及 Uri.parse 失败时的命令回退约定。
    if (!args || typeof args.uri !== 'string') {
        return undefined;
    }
    if (typeof args.line !== 'number' || typeof args.character !== 'number') {
        return undefined;
    }

    return {
        uri: vscode.Uri.parse(args.uri),
        line: args.line,
        character: args.character,
    };
}

/** 按 VS Code 命令 URI 的参数数组协议传递 URI 与零基位置。 */
function buildCommandUri(target: RichHoverTarget): vscode.Uri {
    return vscode.Uri.parse(
        `command:${ZR_RICH_HOVER_FOCUS_COMMAND}?${encodeURIComponent(JSON.stringify([{
            uri: target.uri.toString(),
            line: target.line,
            character: target.character,
        }]))}`,
    );
}

/** 侧栏定位文字使用人可读的一基行列，协议请求仍保留零基。 */
function describeTarget(target: RichHoverTarget): string {
    return `${target.uri.toString()}:${target.line + 1}:${target.character + 1}`;
}

/** 仅在服务端提供符号范围时约束 Hover 高亮；无范围时由编辑器自行决定。 */
function deserializeRange(range: SerializedRange | undefined): vscode.Range | undefined {
    if (!range) {
        return undefined;
    }

    return new vscode.Range(
        range.start.line,
        range.start.character,
        range.end.line,
        range.end.character,
    );
}

/** 移除本扩展注入的侧栏命令，避免普通 Hover 的降级文本再带导航链接。 */
function stripCommandLinks(text: string): string {
    // BUG: 普通 Hover 文档若有一行恰好提到 command:zr.richHover，
    // 此行会作为“本扩展链接”整体删掉；注入链接前独立的 --- 分隔行反而被保留，
    // 使旧服务端侧栏降级内容丢字或多出分隔线。
    return text
        .split(/\r?\n/)
        .filter((line) => !line.includes(COMMAND_MARKER))
        .join('\n')
        .trim();
}

/** 侧栏跟随 Zr 文本及作为 JSON 打开的 .zrp 项目文件，与客户端 selector 保持一致。 */
function isSupportedDocument(document: vscode.TextDocument): boolean {
    return document.languageId === SUPPORTED_LANGUAGE_ID ||
        document.uri.path.toLowerCase().endsWith(SUPPORTED_EXTENSION);
}

/** 激活、无编辑器及首次创建视图时共享的无目标提示。 */
function emptyRenderModel(): RichHoverRenderModel {
    return {
        title: 'Rich Hover',
        sections: [],
        status: 'Move the caret onto a symbol to inspect its semantic details.',
    };
}

/** 标准 Hover 降级时使用 URI 路径末段作标题，不假设桌面文件系统路径。 */
function lastPathSegment(uriPath: string): string {
    const normalized = uriPath.replace(/[\\/]+/g, '/');
    const segments = normalized.split('/').filter((segment) => segment.length > 0);
    return segments[segments.length - 1] ?? normalized;
}
