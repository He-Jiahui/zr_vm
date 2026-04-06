export type BuiltinSymbolKind = 'constant' | 'function' | 'type';

export interface BuiltinSymbolSnapshot {
    name: string;
    kind: BuiltinSymbolKind;
    detail?: string;
}

export interface BuiltinModuleLinkSnapshot {
    name: string;
    moduleName: string;
    detail?: string;
}

export interface BuiltinModuleSnapshot {
    moduleName: string;
    detail?: string;
    modules?: BuiltinModuleLinkSnapshot[];
    symbols?: BuiltinSymbolSnapshot[];
}

const BUILTIN_MODULES: Record<string, BuiltinModuleSnapshot> = {
    'zr.system': {
        moduleName: 'zr.system',
        detail: 'System native module root that aggregates leaf submodules.',
        modules: [
            { name: 'console', moduleName: 'zr.system.console', detail: 'Console output helpers.' },
            { name: 'fs', moduleName: 'zr.system.fs', detail: 'Filesystem helpers.' },
            { name: 'env', moduleName: 'zr.system.env', detail: 'Environment helpers.' },
            { name: 'process', moduleName: 'zr.system.process', detail: 'Process helpers.' },
            { name: 'gc', moduleName: 'zr.system.gc', detail: 'Garbage-collection controls.' },
            { name: 'exception', moduleName: 'zr.system.exception', detail: 'Exception hierarchy and global hooks.' },
            { name: 'vm', moduleName: 'zr.system.vm', detail: 'VM inspection and module invocation helpers.' },
        ],
    },
    'zr.system.console': {
        moduleName: 'zr.system.console',
        symbols: [
            { name: 'print', kind: 'function' },
            { name: 'printLine', kind: 'function' },
            { name: 'printError', kind: 'function' },
            { name: 'printErrorLine', kind: 'function' },
            { name: 'read', kind: 'function' },
            { name: 'readLine', kind: 'function' },
        ],
    },
    'zr.system.fs': {
        moduleName: 'zr.system.fs',
        symbols: [
            { name: 'currentDirectory', kind: 'function' },
            { name: 'changeCurrentDirectory', kind: 'function' },
            { name: 'pathExists', kind: 'function' },
            { name: 'isFile', kind: 'function' },
            { name: 'isDirectory', kind: 'function' },
            { name: 'createDirectory', kind: 'function' },
            { name: 'createDirectories', kind: 'function' },
            { name: 'removePath', kind: 'function' },
            { name: 'readText', kind: 'function' },
            { name: 'writeText', kind: 'function' },
            { name: 'appendText', kind: 'function' },
            { name: 'getInfo', kind: 'function' },
            { name: 'SystemFileInfo', kind: 'type' },
            { name: 'FileSystemEntry', kind: 'type' },
            { name: 'File', kind: 'type' },
            { name: 'Folder', kind: 'type' },
            { name: 'IStreamReader', kind: 'type' },
            { name: 'IStreamWriter', kind: 'type' },
            { name: 'FileStream', kind: 'type' },
        ],
    },
    'zr.system.env': {
        moduleName: 'zr.system.env',
        symbols: [
            { name: 'getVariable', kind: 'function' },
        ],
    },
    'zr.system.process': {
        moduleName: 'zr.system.process',
        symbols: [
            { name: 'arguments', kind: 'constant' },
            { name: 'sleepMilliseconds', kind: 'function' },
            { name: 'exit', kind: 'function' },
        ],
    },
    'zr.system.gc': {
        moduleName: 'zr.system.gc',
        symbols: [
            { name: 'start', kind: 'function' },
            { name: 'stop', kind: 'function' },
            { name: 'step', kind: 'function' },
            { name: 'collect', kind: 'function' },
        ],
    },
    'zr.system.exception': {
        moduleName: 'zr.system.exception',
        symbols: [
            { name: 'registerUnhandledException', kind: 'function' },
            { name: 'Error', kind: 'type' },
            { name: 'StackFrame', kind: 'type' },
            { name: 'RuntimeError', kind: 'type' },
            { name: 'IOException', kind: 'type' },
            { name: 'TypeError', kind: 'type' },
            { name: 'MemoryError', kind: 'type' },
            { name: 'ExceptionError', kind: 'type' },
        ],
    },
    'zr.system.vm': {
        moduleName: 'zr.system.vm',
        symbols: [
            { name: 'loadedModules', kind: 'function' },
            { name: 'state', kind: 'function' },
            { name: 'callModuleExport', kind: 'function' },
            { name: 'SystemVmState', kind: 'type' },
            { name: 'SystemLoadedModuleInfo', kind: 'type' },
        ],
    },
    'zr.math': {
        moduleName: 'zr.math',
        detail: 'Built-in numeric algorithms, vector and matrix types, complex values, quaternions and tensors.',
        symbols: [
            { name: 'PI', kind: 'constant' },
            { name: 'TAU', kind: 'constant' },
            { name: 'E', kind: 'constant' },
            { name: 'EPSILON', kind: 'constant' },
            { name: 'INF', kind: 'constant' },
            { name: 'NAN', kind: 'constant' },
            { name: 'abs', kind: 'function' },
            { name: 'invokeCallback', kind: 'function' },
            { name: 'Vector2', kind: 'type' },
            { name: 'Vector3', kind: 'type' },
            { name: 'Vector4', kind: 'type' },
            { name: 'Quaternion', kind: 'type' },
            { name: 'Complex', kind: 'type' },
            { name: 'Matrix3x3', kind: 'type' },
            { name: 'Matrix4x4', kind: 'type' },
            { name: 'Tensor', kind: 'type' },
        ],
    },
    'zr.network': {
        moduleName: 'zr.network',
        detail: 'Network native module root that aggregates TCP and UDP leaf modules.',
        modules: [
            { name: 'tcp', moduleName: 'zr.network.tcp', detail: 'TCP client and server primitives.' },
            { name: 'udp', moduleName: 'zr.network.udp', detail: 'UDP datagram primitives.' },
        ],
    },
};

export function getBuiltinModuleSnapshot(moduleName: string): BuiltinModuleSnapshot | undefined {
    return BUILTIN_MODULES[moduleName];
}
