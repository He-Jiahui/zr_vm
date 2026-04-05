import type * as vscode from 'vscode';

export type DapRequest = vscode.DebugProtocolMessage & {
    seq: number;
    type: 'request';
    command: string;
    arguments?: Record<string, unknown>;
};

export type DapResponse = vscode.DebugProtocolMessage & {
    seq: number;
    type: 'response';
    request_seq: number;
    success: boolean;
    command: string;
    body?: unknown;
    message?: string;
};

export type DapEvent = vscode.DebugProtocolMessage & {
    seq: number;
    type: 'event';
    event: string;
    body?: unknown;
};

export interface ZrLaunchRequestArguments {
    project: string;
    cwd?: string;
    executionMode?: 'interp' | 'binary';
    cliPath?: string;
    args?: string[];
    debugAddress?: string;
    stopOnEntry?: boolean;
    authToken?: string;
}

export interface ZrAttachRequestArguments {
    endpoint: string;
    authToken?: string;
}

export interface ZrDbgEventMessage {
    method: string;
    params?: Record<string, unknown>;
}

export interface ZrDbgResponseMessage {
    id: number;
    result?: Record<string, unknown>;
    error?: {
        code: number;
        message: string;
    };
}

export interface ZrDbgBreakpoint {
    verified?: boolean;
    line?: number;
    instructionIndex?: number;
}

export interface ZrDbgFrame {
    frameId: number;
    functionName: string;
    sourceFile: string;
    line: number;
    instructionIndex: number;
}

export interface ZrDbgScope {
    scopeId: number;
    frameId: number;
    name: string;
}

export interface ZrDbgVariable {
    name: string;
    type: string;
    value: string;
    variablesReference: number;
}
