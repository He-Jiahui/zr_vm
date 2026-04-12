export type RichHoverSection = {
    role?: string;
    label?: string;
    value?: string;
};

export type RichHoverPayload = {
    sections?: RichHoverSection[];
    range?: unknown;
};

export type RichHoverSummary = {
    title: string;
    lines: string[];
};

export type RichHoverRenderModel = {
    title: string;
    subtitle?: string;
    sections: RichHoverSection[];
    status?: string;
};

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

    if (lines.length === 0 && docsSection?.value) {
        lines.push(truncateSingleLine(docsSection.value, 160));
    }

    return {
        title,
        lines,
    };
}

export function renderRichHoverHtml(model: RichHoverRenderModel): string {
    const title = escapeHtml(model.title || 'Rich Hover');
    const subtitle = model.subtitle ? `<div class="subtitle">${escapeHtml(model.subtitle)}</div>` : '';
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

export function normalizeRichHoverSections(sections: RichHoverSection[]): RichHoverSection[] {
    return sections.filter((section) =>
        typeof section?.value === 'string' &&
        section.value.trim().length > 0,
    );
}

function findSectionByRole(sections: RichHoverSection[], role: string): RichHoverSection | undefined {
    return sections.find((section) => section.role === role);
}

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

function truncateSingleLine(value: string, limit: number): string {
    const singleLine = value.replace(/\s+/g, ' ').trim();
    if (singleLine.length <= limit) {
        return singleLine;
    }

    return `${singleLine.slice(0, Math.max(0, limit - 1)).trimEnd()}…`;
}

function firstNonEmpty(...values: (string | undefined)[]): string {
    for (const value of values) {
        if (value && value.trim().length > 0) {
            return value;
        }
    }

    return '';
}

function escapeHtml(value: string): string {
    return value
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
}
