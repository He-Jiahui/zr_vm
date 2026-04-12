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

export const ZR_RICH_HOVER_VIEW_ID = 'zrRichHover';
export const ZR_RICH_HOVER_FOCUS_COMMAND = 'zr.richHover.focus';
export const ZR_RICH_HOVER_REFRESH_COMMAND = 'zr.richHover.refresh';

const ZR_RICH_HOVER_REQUEST = 'zr/richHover';
const ZR_WORKBENCH_VIEW_COMMAND = 'workbench.view.extension.zr';
const REFRESH_DEBOUNCE_MS = 150;
const SUPPORTED_LANGUAGE_ID = 'zr';
const SUPPORTED_EXTENSION = '.zrp';
const COMMAND_MARKER = 'command:zr.richHover';

type SerializedPosition = {
    line: number;
    character: number;
};

type SerializedRange = {
    start: SerializedPosition;
    end: SerializedPosition;
};

type RichHoverTarget = {
    uri: vscode.Uri;
    line: number;
    character: number;
};

type RichHoverPayloadWithRange = RichHoverPayload & {
    range?: SerializedRange;
};

type RichHoverCommandArgs = {
    uri?: string;
    line?: number;
    character?: number;
    preserveFocus?: boolean;
};

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

export interface ZrRichHoverController extends vscode.Disposable {
    createMiddleware(): HoverMiddleware;
    refresh(): Promise<void>;
}

export { renderRichHoverHtml, summarizeRichHover, type RichHoverSection } from './richHoverShared';

export function registerRichHoverSupport(context: vscode.ExtensionContext): ZrRichHoverController {
    return new ZrRichHoverService(context);
}

class ZrRichHoverService implements ZrRichHoverController, vscode.WebviewViewProvider {
    private readonly disposables: vscode.Disposable[] = [];
    private webviewView: vscode.WebviewView | undefined;
    private refreshChain: Promise<void> = Promise.resolve();
    private refreshTimer: ReturnType<typeof setTimeout> | undefined;
    private explicitTarget: RichHoverTarget | undefined;
    private currentModel: RichHoverRenderModel = emptyRenderModel();

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
            onDidChangeLanguageClient(() => {
                this.scheduleRefresh();
            }),
        );

        context.subscriptions.push(...this.disposables);
        void this.refresh();
    }

    resolveWebviewView(webviewView: vscode.WebviewView): void | Thenable<void> {
        this.webviewView = webviewView;
        webviewView.webview.options = {
            enableScripts: false,
        };
        this.renderCurrentModel();
    }

    createMiddleware(): HoverMiddleware {
        return {
            provideHover: async (document, position, token, next) => {
                const target = toTarget(document, position);
                const payload = await fetchRichHoverPayload(target);

                if (token.isCancellationRequested) {
                    return next(document, position, token);
                }

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

    async refresh(): Promise<void> {
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

    private async focus(args?: RichHoverCommandArgs): Promise<void> {
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

    private scheduleRefresh(): void {
        if (this.refreshTimer !== undefined) {
            clearTimeout(this.refreshTimer);
        }

        this.refreshTimer = setTimeout(() => {
            this.refreshTimer = undefined;
            void this.refresh();
        }, REFRESH_DEBOUNCE_MS);
    }

    private async loadRenderModel(): Promise<RichHoverRenderModel> {
        const target = this.resolveTarget();
        if (!target) {
            return emptyRenderModel();
        }

        const payload = await fetchRichHoverPayload(target);
        const sections = normalizeRichHoverSections(payload?.sections ?? []);
        if (sections.length > 0) {
            const summary = summarizeRichHover(payload);
            return {
                title: summary.title,
                subtitle: describeTarget(target),
                sections,
            };
        }

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

    private renderCurrentModel(): void {
        if (!this.webviewView) {
            return;
        }

        this.webviewView.webview.html = renderRichHoverHtml(this.currentModel);
    }
}

function buildSummaryHover(target: RichHoverTarget, payload: RichHoverPayloadWithRange): vscode.Hover {
    const summary = summarizeRichHover(payload);
    const markdown = new vscode.MarkdownString();
    const commandUri = buildCommandUri(target);

    markdown.isTrusted = {
        enabledCommands: [
            ZR_RICH_HOVER_FOCUS_COMMAND,
            ZR_RICH_HOVER_REFRESH_COMMAND,
        ],
    };
    markdown.supportThemeIcons = true;
    markdown.appendMarkdown(summary.lines.join('\n\n'));
    markdown.appendMarkdown('\n\n---\n\n');
    markdown.appendMarkdown(`[Open rich panel](${commandUri})`);

    return new vscode.Hover(markdown, deserializeRange(payload.range));
}

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

async function fetchRichHoverPayload(target: RichHoverTarget): Promise<RichHoverPayloadWithRange | undefined> {
    const result = await sendLanguageServerRequest<RichHoverPayloadWithRange | null>(ZR_RICH_HOVER_REQUEST, {
        uri: target.uri.toString(),
        line: target.line,
        character: target.character,
    });

    if (!result || typeof result !== 'object') {
        return undefined;
    }

    return result;
}

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

function toTarget(document: vscode.TextDocument, position: vscode.Position): RichHoverTarget {
    return {
        uri: document.uri,
        line: position.line,
        character: position.character,
    };
}

function parseCommandTarget(args?: RichHoverCommandArgs): RichHoverTarget | undefined {
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

function buildCommandUri(target: RichHoverTarget): vscode.Uri {
    return vscode.Uri.parse(
        `command:${ZR_RICH_HOVER_FOCUS_COMMAND}?${encodeURIComponent(JSON.stringify([{
            uri: target.uri.toString(),
            line: target.line,
            character: target.character,
        }]))}`,
    );
}

function describeTarget(target: RichHoverTarget): string {
    return `${target.uri.toString()}:${target.line + 1}:${target.character + 1}`;
}

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

function stripCommandLinks(text: string): string {
    return text
        .split(/\r?\n/)
        .filter((line) => !line.includes(COMMAND_MARKER))
        .join('\n')
        .trim();
}

function isSupportedDocument(document: vscode.TextDocument): boolean {
    return document.languageId === SUPPORTED_LANGUAGE_ID ||
        document.uri.path.toLowerCase().endsWith(SUPPORTED_EXTENSION);
}

function emptyRenderModel(): RichHoverRenderModel {
    return {
        title: 'Rich Hover',
        sections: [],
        status: 'Move the caret onto a symbol to inspect its semantic details.',
    };
}

function lastPathSegment(uriPath: string): string {
    const normalized = uriPath.replace(/[\\/]+/g, '/');
    const segments = normalized.split('/').filter((segment) => segment.length > 0);
    return segments[segments.length - 1] ?? normalized;
}
