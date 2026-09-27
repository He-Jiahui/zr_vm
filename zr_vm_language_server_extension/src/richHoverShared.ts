/** 原生 stdio 与 WASM 共用的结构化 Hover 节；role 决定侧栏色彩和摘要字段。 */
export type RichHoverSection = {
    role?: string;
    label?: string;
    value?: string;
};

/** 扩展请求的最小响应形状；range 由宿主层按 VS Code 类型单独转换。 */
export type RichHoverPayload = {
    sections?: RichHoverSection[];
    range?: unknown;
};

/** 编辑器普通 Hover 的简版内容，与侧栏完整 sections 分开呈现。 */
export type RichHoverSummary = {
    title: string;
    lines: string[];
};

/** 侧栏的宿主无关渲染模型；无目标时 sections 为空且 status 给出说明。 */
export type RichHoverRenderModel = {
    title: string;
    subtitle?: string;
    sections: RichHoverSection[];
    status?: string;
};

/** 选择本项目服务端约定的代表性 role，并压缩普通 Hover 的文档字段。 */
export function summarizeRichHover(payload: RichHoverPayload | null | undefined): RichHoverSummary {
    const sections = normalizeRichHoverSections(payload?.sections ?? []);
    const nameSection = findSectionByRole(sections, 'name');
    const kindSection = findSectionByRole(sections, 'kind');
    const signatureSection = findSectionByRole(sections, 'signature');
    const resolvedTypeSection = findSectionByRole(sections, 'resolvedType');
    const accessSection = findSectionByRole(sections, 'access');
    const sourceSection = findSectionByRole(sections, 'source');
    const docsSection = findSectionByRole(sections, 'docs');
    const lines: string[] = [];
    const title = firstNonEmpty(
        nameSection?.value,
        kindSection?.value,
        'Rich Hover',
    );

    // 名称优先作为标题；仅有 kind 的响应仍能向编辑器提供可读摘要。
    if (nameSection?.value) {
        lines.push(`**${nameSection.label || 'Symbol'}**: ${nameSection.value}`);
    } else if (kindSection?.value) {
        if (kindSection.label && kindSection.label !== 'Kind') {
            lines.push(`**${kindSection.label}**: ${kindSection.value}`);
        } else {
            lines.push(`**${kindSection.value}**`);
        }
    }

    if (signatureSection?.value) {
        lines.push(`Signature: \`${signatureSection.value}\``);
    }
    if (resolvedTypeSection?.value) {
        lines.push(`Resolved Type: \`${resolvedTypeSection.value}\``);
    }
    if (accessSection?.value) {
        lines.push(`Access: \`${accessSection.value}\``);
    }
    if (sourceSection?.value) {
        lines.push(`Source: \`${sourceSection.value}\``);
    }

    // 侧栏保留完整 docs，编辑器提示将文档折叠成单行并截断，避免长文遮住代码。
    if (docsSection?.value) {
        lines.push(truncateSingleLine(docsSection.value, 160));
    }

    return {
        title,
        lines,
    };
}

/** 为禁脚本 Webview 生成完整 HTML；所有服务端文本都在插入前转义。 */
export function renderRichHoverHtml(model: RichHoverRenderModel): string {
    const title = escapeHtml(model.title || 'Rich Hover');
    const subtitle = model.subtitle ? `<div class="subtitle">${escapeHtml(model.subtitle)}</div>` : '';
    // 有结构化节时呈现详情；否则保留宿主提供的无信息或初始空态。
    const body = model.sections.length > 0
        ? model.sections.map((section) => renderSection(section)).join('\n')
        : `<div class="empty">${escapeHtml(model.status || 'Move the caret onto a symbol to inspect it.')}</div>`;

    return `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8" />
<meta name="viewport" content="width=device-width, initial-scale=1.0" />
<style>
    :root {
        color-scheme: light dark;
    }

    body {
        margin: 0;
        padding: 14px;
        color: var(--vscode-editor-foreground);
        background: var(--vscode-sideBar-background);
        font-family: var(--vscode-font-family);
    }

    .header {
        margin-bottom: 14px;
        padding-bottom: 10px;
        border-bottom: 1px solid var(--vscode-sideBarSectionHeader-border, transparent);
    }

    .title {
        font-size: 16px;
        font-weight: 700;
        line-height: 1.25;
        color: var(--vscode-sideBarTitle-foreground, var(--vscode-editor-foreground));
    }

    .subtitle {
        margin-top: 4px;
        font-size: 12px;
        color: var(--vscode-descriptionForeground);
        word-break: break-all;
    }

    .section {
        padding: 10px 0;
        border-bottom: 1px solid var(--vscode-sideBarSectionHeader-border, rgba(127, 127, 127, 0.2));
    }

    .section:last-child {
        border-bottom: none;
    }

    .label {
        margin-bottom: 4px;
        font-size: 11px;
        font-weight: 600;
        letter-spacing: 0.04em;
        text-transform: uppercase;
        color: var(--vscode-descriptionForeground);
    }

    .value {
        white-space: pre-wrap;
        word-break: break-word;
        line-height: 1.45;
        color: var(--section-color, var(--vscode-editor-foreground));
    }

    .value.code {
        font-family: var(--vscode-editor-font-family);
    }

    .role-name {
        --section-color: var(--vscode-symbolIcon-functionForeground, var(--vscode-textLink-foreground));
    }

    .role-kind {
        --section-color: var(--vscode-symbolIcon-classForeground, var(--vscode-textLink-foreground));
    }

    .role-signature {
        --section-color: var(--vscode-textLink-foreground);
    }

    .role-resolvedType {
        --section-color: var(--vscode-terminal-ansiGreen, var(--vscode-editor-foreground));
    }

    .role-access {
        --section-color: var(--vscode-terminal-ansiYellow, var(--vscode-editor-foreground));
    }

    .role-category {
        --section-color: var(--vscode-terminal-ansiBlue, var(--vscode-editor-foreground));
    }

    .role-applicableTo {
        --section-color: var(--vscode-terminal-ansiCyan, var(--vscode-editor-foreground));
    }

    .role-source {
        --section-color: var(--vscode-terminal-ansiMagenta, var(--vscode-editor-foreground));
    }

    .role-detail {
        --section-color: var(--vscode-editor-foreground);
    }

    .role-docs {
        --section-color: var(--vscode-editor-foreground);
    }

    .empty {
        padding: 10px 0;
        color: var(--vscode-descriptionForeground);
        line-height: 1.5;
    }
</style>
</head>
<body>
    <div class="header">
        <div class="title">${title}</div>
        ${subtitle}
    </div>
    ${body}
</body>
</html>`;
}

/** 仅让有非空文本的节参与摘要和侧栏，兼容服务端返回可选字段。 */
export function normalizeRichHoverSections(sections: RichHoverSection[]): RichHoverSection[] {
    return sections.filter((section) =>
        typeof section?.value === 'string' &&
        section.value.trim().length > 0,
    );
}

/** 摘要只取同 role 的第一节，保留协议顺序与原生序列化顺序。 */
function findSectionByRole(sections: RichHoverSection[], role: string): RichHoverSection | undefined {
    return sections.find((section) => section.role === role);
}

/** 映射语义 role 到主题色，并在属性与正文位置分别插入已转义的文本。 */
function renderSection(section: RichHoverSection): string {
    const role = escapeHtml(section.role || 'detail');
    const label = escapeHtml(section.label || 'Detail');
    const value = escapeHtml(section.value || '');
    const codeClass = section.role === 'signature' || section.role === 'resolvedType' ? ' code' : '';

    return `<section class="section role-${role}">
    <div class="label">${label}</div>
    <div class="value${codeClass}">${value}</div>
</section>`;
}

/** 将文档摘要压成一行并截断；完整文档仍由侧栏模型持有。 */
function truncateSingleLine(value: string, limit: number): string {
    const singleLine = value.replace(/\s+/g, ' ').trim();
    if (singleLine.length <= limit) {
        return singleLine;
    }

    return `${singleLine.slice(0, Math.max(0, limit - 1)).trimEnd()}…`;
}

/** 原生响应缺少 name 时选择 kind，再使用固定视图标题。 */
function firstNonEmpty(...values: (string | undefined)[]): string {
    for (const value of values) {
        if (value && value.trim().length > 0) {
            return value;
        }
    }

    return '';
}

/** 封闭 Webview HTML 的文本与属性边界，避免源码文档影响侧栏结构。 */
function escapeHtml(value: string): string {
    return value
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
}
