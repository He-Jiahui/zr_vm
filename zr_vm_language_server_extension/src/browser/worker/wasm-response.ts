import { ErrorCodes, ResponseError } from 'vscode-languageserver/browser';
import type { WasmResponse } from './wasm-bridge';

export function responseData<T>(response: WasmResponse<T>, fallback: T): T {
    if (typeof response !== 'object' || response === null || typeof response.success !== 'boolean') {
        throw new ResponseError(ErrorCodes.InternalError, 'Malformed WASM response envelope.');
    }
    if (response.success === false) {
        const code = typeof response.code === 'number' && Number.isInteger(response.code)
            ? response.code : ErrorCodes.InternalError;
        const message = typeof response.error === 'string' && response.error.length > 0
            ? response.error : 'Malformed WASM error response.';
        throw new ResponseError(code, message, response.data);
    }
    if (!Object.prototype.hasOwnProperty.call(response, 'data') || response.data === undefined ||
        'error' in response || 'code' in response) {
        throw new ResponseError(ErrorCodes.InternalError, 'Malformed WASM success response.');
    }
    return response.data ?? fallback;
}
