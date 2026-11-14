import * as vscode from 'vscode';
import type { LanguageClient, DiagnosticProviderMiddleware, vsdiag } from 'vscode-languageclient/node';
import { startDiagnosticWorker, type DiagnosticWorker, type DiagnosticWorkerContext } from './diagnosticWorkerProcess';

export type { DiagnosticWorker } from './diagnosticWorkerProcess';
export interface DiagnosticWorkerOptions {
    workerFactory?: (serverPath: string, output: vscode.OutputChannel, context: DiagnosticWorkerContext) => DiagnosticWorker;
    maxWorkers?: number;
}
type BufferSnapshot = { uri: string; languageId: string; version: number; text: string };
type Slot = {
    worker: DiagnosticWorker;
    documents: Map<string, BufferSnapshot>;
    notifications: Promise<void>;
    job?: Job;
    retiring?: Promise<void>;
};
type Job = {
    method: string; params: any; key: string; revision: number;
    token: vscode.CancellationToken; subscription?: vscode.Disposable; slot?: Slot; settled: boolean;
    resolve(value: any): void; reject(error: unknown): void;
};

const isZrDocument = (document: vscode.TextDocument) => /\.zrp?$/i.test(document.uri.toString());

/** Two isolated native processes keep expensive lint away from interactive requests. */
export function createDiagnosticWorkers(
    serverPath: string, output: vscode.OutputChannel, getSelectedProjectUri: () => string | undefined,
    options: DiagnosticWorkerOptions = {},
) {
    const slots: Slot[] = [];
    const queue: Job[] = [];
    const affinity = new Map<string, Slot>();
    const factory = options.workerFactory ?? startDiagnosticWorker;
    const limit = Math.max(1, Math.min(2, options.maxWorkers ?? 2));
    let selectedProjectUri = getSelectedProjectUri();
    let configuration: unknown;
    let revision = 0;
    let workspaceSnapshot: { items: vsdiag.WorkspaceDocumentDiagnosticReport[]; expiresAt: number } | undefined;
    let disposed = false;
    let disposal: Promise<void> | undefined;

    function finish(job: Job, error?: unknown, result?: unknown): void {
        if (job.settled) { return; }
        job.settled = true;
        job.subscription?.dispose();
        if (error) { job.reject(error); } else { job.resolve(result); }
    }

    function retire(slot: Slot): Promise<void> {
        if (slot.retiring) { return slot.retiring; }
        // Retiring processes count against the bound until they really exit.
        slot.retiring = Promise.resolve().then(() => slot.worker.dispose()).then(() => {
            slots.splice(slots.indexOf(slot), 1);
            for (const [key, owner] of affinity) { if (owner === slot) { affinity.delete(key); } }
            pump();
        }, error => {
            output.appendLine(`[diagnostics] process cleanup failed: ${String(error)}`);
            throw error;
        });
        void slot.retiring.catch(() => {});
        return slot.retiring;
    }

    function cancel(job: Job): void {
        if (job.settled) { return; }
        const index = queue.indexOf(job);
        if (index >= 0) { queue.splice(index, 1); }
        finish(job, new vscode.CancellationError());
        if (job.slot) { void retire(job.slot); }
    }

    function invalidate(): void {
        revision++;
        workspaceSnapshot = undefined;
        for (const job of [...queue]) { cancel(job); }
        for (const slot of slots) { if (slot.job) { cancel(slot.job); } }
    }

    const subscriptions = [
        vscode.workspace.onDidOpenTextDocument(document => { if (isZrDocument(document)) { invalidate(); } }),
        vscode.workspace.onDidChangeTextDocument(event => {
            if (isZrDocument(event.document) && event.contentChanges.length) { invalidate(); }
        }),
        vscode.workspace.onDidCloseTextDocument(document => { if (isZrDocument(document)) { invalidate(); } }),
    ];

    /** Snapshot all open dependencies, including unsaved contents, before a pull. */
    async function synchronize(slot: Slot): Promise<void> {
        const current = new Map(vscode.workspace.textDocuments.filter(isZrDocument).map(document => {
            const uri = document.uri.toString();
            return [uri, { uri, languageId: document.languageId, version: document.version, text: document.getText() }] as const;
        }));
        for (const uri of slot.documents.keys()) {
            if (!current.has(uri)) {
                await slot.worker.notify('textDocument/didClose', { textDocument: { uri } });
                slot.documents.delete(uri);
            }
        }
        for (const [uri, document] of current) {
            const previous = slot.documents.get(uri);
            if (!previous) {
                await slot.worker.notify('textDocument/didOpen', { textDocument: document });
            } else if (document.version <= previous.version && previous.text !== document.text) {
                // A closed and reopened editor can reuse its old version number.
                await slot.worker.notify('textDocument/didClose', { textDocument: { uri } });
                await slot.worker.notify('textDocument/didOpen', { textDocument: document });
            } else if (previous.version !== document.version || previous.text !== document.text) {
                await slot.worker.notify('textDocument/didChange', {
                    textDocument: { uri, version: document.version }, contentChanges: [{ text: document.text }],
                });
            }
            slot.documents.set(uri, document);
        }
    }

    async function execute(slot: Slot, job: Job): Promise<void> {
        try {
            await slot.notifications;
            await synchronize(slot);
            if (job.settled || job.revision !== revision) { cancel(job); return; }
            const result = await slot.worker.request(job.method, job.params, job.token);
            if (job.token.isCancellationRequested || job.revision !== revision) { cancel(job); return; }
            finish(job, undefined, result);
        } catch (error) {
            if (!job.settled) {
                finish(job, error);
                void retire(slot);
            }
        } finally {
            if (slot.job === job) { slot.job = undefined; }
            pump();
        }
    }

    function forward(slot: Slot, method: string, params: unknown): void {
        slot.notifications = slot.notifications.then(async () => { await slot.worker.notify(method, params); });
        void slot.notifications.catch(error => {
            if (disposed || slot.retiring) { return; }
            output.appendLine(`[diagnostics] notification failed: ${String(error)}`);
            void retire(slot);
        });
    }

    function pump(): void {
        if (disposed) { return; }
        while (queue.length) {
            const job = queue[0];
            const preferred = affinity.get(job.key);
            let slot = preferred && !preferred.job && !preferred.retiring ? preferred : slots.find(item => !item.job && !item.retiring);
            if (!slot && slots.length < limit) {
                try {
                    const context = { selectedProjectUri, workspaceFolders: (vscode.workspace.workspaceFolders ?? []).map(folder => ({ uri: folder.uri.toString(), name: folder.name })) };
                    slot = { worker: factory(serverPath, output, context), documents: new Map(), notifications: Promise.resolve() };
                    slots.push(slot);
                    if (configuration !== undefined) {
                        forward(slot, 'workspace/didChangeConfiguration', configuration);
                    }
                } catch (error) { queue.shift(); finish(job, error); continue; }
            }
            if (!slot) { return; }
            queue.shift();
            if (job.token.isCancellationRequested || job.revision !== revision) { cancel(job); continue; }
            slot.job = job;
            job.slot = slot;
            affinity.set(job.key, slot);
            void execute(slot, job);
        }
    }

    function request(method: string, params: any, token: vscode.CancellationToken): Promise<any> {
        if (disposed || token.isCancellationRequested) { return Promise.reject(new vscode.CancellationError()); }
        return new Promise((resolve, reject) => {
            const job: Job = { method, params, key: params.textDocument?.uri ?? 'workspace', revision, token, resolve, reject, settled: false };
            queue.push(job);
            // Visible document requests precede the background workspace sweep.
            queue.sort((left, right) => Number(left.method === 'workspace/diagnostic') - Number(right.method === 'workspace/diagnostic'));
            job.subscription = token.onCancellationRequested(() => cancel(job));
            if (token.isCancellationRequested) { cancel(job); } else { pump(); }
        });
    }

    function notify(method: string, params: any): void {
        if (disposed) { return; }
        if (method === 'workspace/didChangeConfiguration') { configuration = params; }
        if (method === 'zr/selectedProject') {
            const next = params?.uri ?? undefined;
            if (next === selectedProjectUri) { return; }
            selectedProjectUri = next;
        }
        invalidate();
        // Idle workers retain caches, while receiving the same invalidation as the main server.
        for (const slot of slots) {
            if (!slot.retiring) {
                forward(slot, method, params);
            }
        }
    }

    function createMiddleware(getClient: () => LanguageClient): DiagnosticProviderMiddleware {
        const convert = async (report: any, token: vscode.CancellationToken): Promise<vsdiag.DocumentDiagnosticReport> => {
            const converted: vsdiag.DocumentDiagnosticReport = report.kind === 'unchanged'
                ? { kind: 'unChanged' as vsdiag.DocumentDiagnosticReportKind.unChanged, resultId: report.resultId }
                : { kind: 'full' as vsdiag.DocumentDiagnosticReportKind.full, resultId: report.resultId,
                    items: await getClient().protocol2CodeConverter.asDiagnostics(report.items ?? [], token) };
            if (report.relatedDocuments) {
                converted.relatedDocuments = {};
                for (const [uri, related] of Object.entries(report.relatedDocuments)) { converted.relatedDocuments[uri] = await convert(related, token); }
            }
            return converted;
        };
        return {
            async provideDiagnostics(document, previousResultId, token) {
                const requestedRevision = revision;
                try {
                    const uri = 'uri' in document ? document.uri : document;
                    const report = await request('textDocument/diagnostic', { textDocument: { uri: uri.toString() }, previousResultId }, token);
                    const result = await convert(report, token);
                    if (token.isCancellationRequested || disposed || requestedRevision !== revision) { throw new vscode.CancellationError(); }
                    return result;
                } catch (error) {
                    if (error instanceof vscode.CancellationError) { throw error; }
                    // Never fall back to the interactive process: that would freeze completion again.
                    output.appendLine(`[diagnostics] document pull failed: ${String(error)}`);
                    return { kind: 'full' as vsdiag.DocumentDiagnosticReportKind.full, items: [] };
                }
            },
            async provideWorkspaceDiagnostics(resultIds, token, reporter) {
                const requestedRevision = revision;
                try {
                    if (token.isCancellationRequested || disposed) { return { items: [] }; }
                    let snapshot = workspaceSnapshot && Date.now() < workspaceSnapshot.expiresAt ? workspaceSnapshot.items : undefined;
                    if (!snapshot) {
                        // SDK IDs may come from the document worker, whose generations are
                        // independent. Capture full reports once per unchanged editor revision.
                        const report = await request('workspace/diagnostic', { previousResultIds: [] }, token);
                        snapshot = [];
                        for (const item of report.items ?? []) {
                            snapshot.push({ ...await convert(item, token), uri: getClient().protocol2CodeConverter.asUri(item.uri), version: item.version ?? null });
                        }
                        if (token.isCancellationRequested || disposed || requestedRevision !== revision) { return { items: [] }; }
                        // Revalidate infrequently even if an external provider or config edit
                        // cannot generate a workspace file event (for example outside a root).
                        workspaceSnapshot = { items: snapshot, expiresAt: Date.now() + 5 * 60 * 1000 };
                    }
                    const previous = new Map(resultIds.map(item => [item.uri.toString(), item.value]));
                    const items = snapshot.map(item => item.resultId !== undefined && item.resultId === previous.get(item.uri.toString())
                        ? { kind: 'unChanged' as vsdiag.DocumentDiagnosticReportKind.unChanged, uri: item.uri, version: item.version, resultId: item.resultId }
                        : item);
                    reporter({ items });
                } catch (error) {
                    if (!(error instanceof vscode.CancellationError)) { throw error; }
                    // SDK 8.1 counts rejected workspace polls toward its failure limit.
                    // An edit invalidates this poll; the SDK will request a fresh one.
                }
                return { items: [] };
            },
        };
    }

    return {
        createMiddleware, notify,
        dispose(): Promise<void> {
            return disposal ??= (async () => {
                disposed = true;
                for (const subscription of subscriptions) { subscription.dispose(); }
                invalidate();
                await Promise.all(slots.map(retire));
            })();
        },
    };
}
