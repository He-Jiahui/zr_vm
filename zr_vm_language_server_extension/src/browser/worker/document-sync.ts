import { LSPErrorCodes, ResponseError } from 'vscode-languageserver/browser';
import type { WasmResponse } from './wasm-bridge';
import { responseData } from './wasm-response';

type Snapshot = Readonly<{ text: string; version: number }>;
type DocumentState = { generation: number; revision: number; openVersion: number; synchronized: boolean; snapshot?: Snapshot };
export type DocumentToken = Readonly<{ uri: string; generation: number; revision: number; version: number }>;
type Backend = {
    updateDocument(uri: string, text: string, version: number): Promise<WasmResponse<unknown>>;
    closeDocument(uri: string): Promise<WasmResponse<unknown>>;
};

export function contentModified(): ResponseError<void> {
    return new ResponseError(LSPErrorCodes.ContentModified, 'Document content is not synchronized.');
}

/** Serializes backend mutations and commits JS snapshots only after successful updates. */
export class DocumentSyncStore {
    private readonly documents = new Map<string, DocumentState>();
    private readonly queues = new Map<string, Promise<void>>();
    private nextGeneration = 0;

    constructor(private readonly backend: Backend) {}

    open(uri: string, text: unknown, version: unknown): Promise<boolean> {
        if (this.documents.has(uri) || !validText(text) || !unsignedInteger(version)) {
            return Promise.resolve(false);
        }
        const state: DocumentState = { generation: ++this.nextGeneration, revision: 0, openVersion: version, synchronized: false };
        this.documents.set(uri, state);
        return this.enqueue(uri, () => this.commit(uri, state, { text, version }));
    }

    change(uri: string, version: unknown, changes: unknown): Promise<boolean> {
        const state = this.documents.get(uri);
        if (!state) { return Promise.resolve(false); }
        // Invalidate outstanding queries/diagnostics as soon as a notification arrives.
        state.revision++;
        return this.enqueue(uri, async () => {
            try {
                if (!unsignedInteger(version) || version <= (state.snapshot?.version ?? state.openVersion) ||
                    !Array.isArray(changes) || changes.length === 0 ||
                    (!state.synchronized && !singleFullReplacement(changes))) {
                    throw contentModified();
                }
                const text = applyContentChanges(state.snapshot?.text ?? '', changes);
                return await this.commit(uri, state, { text, version });
            } catch {
                state.synchronized = false;
                return false;
            }
        });
    }

    close(uri: string): Promise<void> {
        // Invalidate the generation before waiting for an in-flight update.
        this.documents.delete(uri);
        return this.enqueue(uri, async () => {
            responseData(await this.backend.closeDocument(uri), null);
        });
    }

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

    isCurrent(token: DocumentToken): boolean {
        const state = this.documents.get(token.uri);
        return state !== undefined && state.synchronized && state.generation === token.generation &&
            state.revision === token.revision && state.snapshot?.version === token.version;
    }

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

    private enqueue<T>(uri: string, action: () => Promise<T>): Promise<T> {
        const operation = (this.queues.get(uri) ?? Promise.resolve()).then(action);
        const tail = operation.then(() => {}, () => {});
        this.queues.set(uri, tail);
        void tail.then(() => { if (this.queues.get(uri) === tail) { this.queues.delete(uri); } });
        return operation;
    }
}

function unsignedInteger(value: unknown): value is number {
    // LSP integer and the WASM update API both use signed 32-bit integers.
    return typeof value === 'number' && Number.isInteger(value) && value >= 0 && value <= 0x7fffffff;
}

function object(value: unknown): value is Record<string, unknown> {
    return typeof value === 'object' && value !== null;
}

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

function singleFullReplacement(changes: unknown[]): boolean {
    return changes.length === 1 && object(changes[0]) && !('range' in changes[0]) && !('rangeLength' in changes[0]);
}

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
