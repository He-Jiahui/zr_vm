import { LSPErrorCodes, ResponseError } from 'vscode-languageserver/browser';
import type { WasmResponse } from './wasm-bridge';
import { responseData } from './wasm-response';

/** 已获 WASM 接受的编辑器文本；失败通知不得覆盖它。 */
type Snapshot = Readonly<{ text: string; version: number }>;
/** 代际隔离关闭重开，修订号隔离同一次打开期间的异步查询。 */
type DocumentState = { generation: number; revision: number; openVersion: number; synchronized: boolean; snapshot?: Snapshot };
/** 查询和诊断携带的快照身份；发送结果前须再次核对。 */
export type DocumentToken = Readonly<{ uri: string; generation: number; revision: number; version: number }>;
/** 同一 URI 的更新与关闭由本层排队，底层仅需提供 WASM 响应封装。 */
type Backend = {
    updateDocument(uri: string, text: string, version: number): Promise<WasmResponse<unknown>>;
    closeDocument(uri: string): Promise<WasmResponse<unknown>>;
};

/** 让未同步或过期的文档请求沿 LSP 错误通道返回 ContentModified。 */
export function contentModified(): ResponseError<void> {
    return new ResponseError(LSPErrorCodes.ContentModified, 'Document content is not synchronized.');
}

/**
 * 维护 Web worker 的已提交文档快照：按 URI 串行化 WASM 变更，
 * 将通知接受、后端成功与查询结果有效性分开判定。
 */
export class DocumentSyncStore {
    private readonly documents = new Map<string, DocumentState>();
    /** 关闭与重开也共用 URI 队列，避免旧关闭覆盖新打开。 */
    private readonly queues = new Map<string, Promise<void>>();
    private nextGeneration = 0;

    constructor(private readonly backend: Backend) {}

    /** didOpen 只接纳一个有效初始快照；重复打开不能覆盖已同步文本。 */
    open(uri: string, text: unknown, version: unknown): Promise<boolean> {
        if (this.documents.has(uri) || !validText(text) || !unsignedInteger(version)) {
            return Promise.resolve(false);
        }
        const state: DocumentState = { generation: ++this.nextGeneration, revision: 0, openVersion: version, synchronized: false };
        this.documents.set(uri, state);
        return this.enqueue(uri, () => this.commit(uri, state, { text, version }));
    }

    /** didChange 立即废弃在途读者；失同步后只允许高于上次已提交版本的单次全文替换恢复。 */
    change(uri: string, version: unknown, changes: unknown): Promise<boolean> {
        const state = this.documents.get(uri);
        if (!state) { return Promise.resolve(false); }
        // 通知到达即改变修订号；不能等 WASM 排队更新完才阻止旧诊断发布。
        state.revision++;
        return this.enqueue(uri, async () => {
            try {
                // 拒绝不完整变更后已无法信任增量编辑的基底；恢复必须重建全文。
                if (!unsignedInteger(version) || version <= (state.snapshot?.version ?? state.openVersion) ||
                    !Array.isArray(changes) || changes.length === 0 ||
                    (!state.synchronized && !singleFullReplacement(changes))) {
                    throw contentModified();
                }
                const text = applyContentChanges(state.snapshot?.text ?? '', changes);
                return await this.commit(uri, state, { text, version });
            } catch {
                // 保留上次提交的文本与版本，但在恢复前禁止任何查询使用它们。
                state.synchronized = false;
                return false;
            }
        });
    }

    /** didClose 先使读者失效，再按队列顺序释放 WASM 文档。 */
    close(uri: string): Promise<void> {
        // 等待在途更新前就撤销代际，供 worker 立即清空诊断并拦截旧结果。
        this.documents.delete(uri);
        return this.enqueue(uri, async () => {
            responseData(await this.backend.closeDocument(uri), null);
        });
    }

    /** 文档查询等待此前写入，并在异步后端查询返回时重验快照身份。 */
    async read<T>(uri: string, query: (snapshot: Snapshot) => Promise<T>): Promise<{ value: T; token: DocumentToken }> {
        const state = this.documents.get(uri);
        const revision = state?.revision;
        await this.queues.get(uri);
        if (!state || state !== this.documents.get(uri) || state.revision !== revision ||
            !state.synchronized || !state.snapshot) { throw contentModified(); }
        const token = { uri, generation: state.generation, revision: state.revision, version: state.snapshot.version };
        const value = await query(state.snapshot);
        if (!this.isCurrent(token)) { throw contentModified(); }
        return { value, token };
    }

    /** 诊断发布者及请求处理者可用同一令牌做最后一次过期检查。 */
    isCurrent(token: DocumentToken): boolean {
        const state = this.documents.get(token.uri);
        return state !== undefined && state.synchronized && state.generation === token.generation &&
            state.revision === token.revision && state.snapshot?.version === token.version;
    }

    /** 后端成功后才提交候选文本；关闭会使旧实例即使完成写入也不可再被读取。 */
    private async commit(uri: string, state: DocumentState, snapshot: Snapshot): Promise<boolean> {
        if (this.documents.get(uri) !== state) { return false; }
        try {
            responseData(await this.backend.updateDocument(uri, snapshot.text, snapshot.version), null);
            state.snapshot = snapshot;
            state.synchronized = true;
            return true;
        } catch (error) {
            state.synchronized = false;
            console.error('[zr-web-worker] document update failed:', uri, error);
            return false;
        }
    }

    /** 使用可恢复的队列尾部保持通知顺序；单次失败不阻塞后续全文恢复。 */
    private enqueue<T>(uri: string, action: () => Promise<T>): Promise<T> {
        const operation = (this.queues.get(uri) ?? Promise.resolve()).then(action);
        const tail = operation.then(() => {}, () => {});
        this.queues.set(uri, tail);
        void tail.then(() => { if (this.queues.get(uri) === tail) { this.queues.delete(uri); } });
        return operation;
    }
}

/** 版本和位置同时满足 LSP 非负整数与 WASM signed 32-bit 入参上限。 */
function unsignedInteger(value: unknown): value is number {
    return typeof value === 'number' && Number.isInteger(value) && value >= 0 && value <= 0x7fffffff;
}

/** 对不可信通知载荷做最小对象检查，后续再按字段契约验证。 */
function object(value: unknown): value is Record<string, unknown> {
    return typeof value === 'object' && value !== null;
}

/** 禁止孤立代理项进入 TextEncoder/WASM，避免无效文本与位置编码分歧。 */
function validText(value: unknown): value is string {
    if (typeof value !== 'string') { return false; }
    for (let i = 0; i < value.length; i++) {
        const code = value.charCodeAt(i);
        if (code >= 0xd800 && code <= 0xdbff) {
            const next = value.charCodeAt(++i);
            if (!(next >= 0xdc00 && next <= 0xdfff)) { return false; }
        } else if (code >= 0xdc00 && code <= 0xdfff) { return false; }
    }
    return true;
}

/** 失同步时只接受没有范围元数据的单条完整文本替换。 */
function singleFullReplacement(changes: unknown[]): boolean {
    return changes.length === 1 && object(changes[0]) && !('range' in changes[0]) && !('rangeLength' in changes[0]);
}

/** 在临时文本上依次验证多条编辑；任一条失败则整次通知不提交。 */
function applyContentChanges(text: string, changes: unknown[]): string {
    let result = text;
    for (const change of changes) {
        if (!object(change) || !validText(change.text)) { throw contentModified(); }
        if (!('range' in change)) {
            if ('rangeLength' in change) { throw contentModified(); }
            result = change.text;
        } else {
            if (!object(change.range)) { throw contentModified(); }
            const start = positionToOffset(result, change.range.start);
            const end = positionToOffset(result, change.range.end);
            if (end < start || ('rangeLength' in change &&
                (!unsignedInteger(change.rangeLength) || change.rangeLength !== end - start))) {
                throw contentModified();
            }
            result = result.slice(0, start) + change.text + result.slice(end);
        }
    }
    return result;
}

/** 将 LSP UTF-16 行列映射到 JS 字符串偏移，排除换行符和代理对内部。 */
function positionToOffset(text: string, position: unknown): number {
    if (!object(position) || !unsignedInteger(position.line) || !unsignedInteger(position.character)) {
        throw contentModified();
    }
    let start = 0;
    for (let line = 0; line < position.line; line++) {
        while (start < text.length && text[start] !== '\r' && text[start] !== '\n') { start++; }
        if (start === text.length) { throw contentModified(); }
        if (text[start++] === '\r' && text[start] === '\n') { start++; }
    }
    let end = start;
    while (end < text.length && text[end] !== '\r' && text[end] !== '\n') { end++; }
    const offset = start + position.character;
    if (offset > end || (offset > start && text.charCodeAt(offset) >= 0xdc00 && text.charCodeAt(offset) <= 0xdfff)) {
        throw contentModified();
    }
    return offset;
}
