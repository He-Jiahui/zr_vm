import * as vscode from 'vscode';
import {
    dirnameFromPath,
    joinNormalizedPath,
    normalizeFilePath,
    parseProjectManifestText,
    pickBestProjectForFile,
    type ParsedProjectManifest,
} from './projectSupport';

const ZR_PROJECT_GLOB = '**/*.zrp';
const ZR_PROJECT_EXCLUDE_GLOB = '**/{build,bin,node_modules,.git,.vscode-test}/**';
const ZR_SELECTED_PROJECT_KEY = 'zr.selectedProjectUri';
const onDidChangeSelectedProjectEmitter = new vscode.EventEmitter<vscode.Uri | undefined>();

export interface WorkspaceProject {
    id: string;
    uri: vscode.Uri;
    workspaceFolder: vscode.WorkspaceFolder;
    manifest: ParsedProjectManifest;
    projectPath: string;
    projectDirectoryPath: string;
    sourceRootPath: string;
    relativePath: string;
    label: string;
}

export const onDidChangeSelectedProject = onDidChangeSelectedProjectEmitter.event;

export async function discoverWorkspaceProjects(): Promise<WorkspaceProject[]> {
    const uris = await vscode.workspace.findFiles(ZR_PROJECT_GLOB, ZR_PROJECT_EXCLUDE_GLOB);
    const projects = await Promise.all(uris.map(async (uri) => createWorkspaceProject(uri)));

    return projects
        .filter((project): project is WorkspaceProject => project !== undefined)
        .sort((left, right) => compareStrings(left.relativePath, right.relativePath));
}

export async function createWorkspaceProject(uri: vscode.Uri): Promise<WorkspaceProject | undefined> {
    const workspaceFolder = vscode.workspace.getWorkspaceFolder(uri);
    if (!workspaceFolder) {
        return undefined;
    }

    const document = await vscode.workspace.openTextDocument(uri);
    const manifest = parseProjectManifestText(document.getText(), uri.fsPath || uri.path);
    if (!manifest) {
        return undefined;
    }

    const projectDirectoryPath = dirnameFromPath(uri.fsPath || uri.path);
    const sourceRootPath = joinNormalizedPath(projectDirectoryPath, manifest.source);
    return {
        id: uri.toString(),
        uri,
        workspaceFolder,
        manifest,
        projectPath: normalizeFilePath(uri.fsPath || uri.path),
        projectDirectoryPath,
        sourceRootPath,
        relativePath: relativePathWithinFolder(workspaceFolder, uri),
        label: manifest.name && manifest.name.length > 0
            ? manifest.name
            : removeExtension(lastPathSegment(uri.path)),
    };
}

export function pickWorkspaceProjectForUri(
    uri: vscode.Uri,
    projects: WorkspaceProject[],
): WorkspaceProject | undefined {
    return pickBestProjectForFile(normalizeFilePath(uri.fsPath || uri.path), projects);
}

export function isZrpUri(uri: vscode.Uri): boolean {
    return uri.path.toLowerCase().endsWith('.zrp');
}

export function isZrpDocument(document: vscode.TextDocument): boolean {
    return isZrpUri(document.uri);
}

export async function resolveProjectUri(
    folder: vscode.WorkspaceFolder | undefined,
    projectPath: unknown,
): Promise<vscode.Uri | undefined> {
    if (typeof projectPath === 'string' && projectPath.trim().length > 0) {
        const resolved = resolveRelativePath(folder, projectPath.trim());
        return vscode.Uri.file(resolved);
    }

    return findProjectFile(folder);
}

export async function resolveSelectedProjectUri(
    context: vscode.ExtensionContext,
    folder: vscode.WorkspaceFolder | undefined,
    allowPrompt: boolean,
): Promise<vscode.Uri | undefined> {
    const projects = await discoverWorkspaceProjects();
    const storedId = context.workspaceState.get<string>(ZR_SELECTED_PROJECT_KEY);
    const activeUri = vscode.window.activeTextEditor?.document.uri;

    if (projects.length === 0) {
        await setSelectedProjectUri(context, undefined);
        return undefined;
    }

    if (activeUri?.scheme === 'file') {
        const activeProject = isZrpUri(activeUri)
            ? projects.find((project) => project.uri.toString() === activeUri.toString())
            : pickWorkspaceProjectForUri(activeUri, projects);
        if (activeProject) {
            await setSelectedProjectUri(context, activeProject.uri);
            return activeProject.uri;
        }
    }

    if (projects.length === 1) {
        await setSelectedProjectUri(context, projects[0].uri);
        return projects[0].uri;
    }

    if (storedId) {
        const storedProject = projects.find((project) => project.id === storedId);
        if (storedProject) {
            return storedProject.uri;
        }
    }

    if (!allowPrompt) {
        return undefined;
    }

    return selectWorkspaceProject(context, folder, projects);
}

export async function resolveSelectedWorkspaceProject(
    context: vscode.ExtensionContext,
    folder: vscode.WorkspaceFolder | undefined,
    allowPrompt: boolean,
): Promise<WorkspaceProject | undefined> {
    const projects = await discoverWorkspaceProjects();
    const selectedUri = await resolveSelectedProjectUri(context, folder, allowPrompt);
    if (!selectedUri) {
        return undefined;
    }

    return projects.find((project) => project.uri.toString() === selectedUri.toString());
}

export async function selectWorkspaceProject(
    context: vscode.ExtensionContext,
    folder: vscode.WorkspaceFolder | undefined,
    preloadedProjects?: WorkspaceProject[],
): Promise<vscode.Uri | undefined> {
    const projects = preloadedProjects ?? await discoverWorkspaceProjects();
    let selectedProject: WorkspaceProject | undefined;

    void folder;

    if (projects.length === 0) {
        await setSelectedProjectUri(context, undefined);
        return undefined;
    }

    if (projects.length === 1) {
        selectedProject = projects[0];
    } else {
        const picked = await vscode.window.showQuickPick(
            projects.map((project) => ({
                label: project.label,
                description: project.relativePath,
                uri: project.uri,
            })),
            {
                title: 'Select ZR project',
            },
        );
        selectedProject = projects.find((project) => project.uri.toString() === picked?.uri.toString());
    }

    if (!selectedProject) {
        return undefined;
    }

    await setSelectedProjectUri(context, selectedProject.uri);
    return selectedProject.uri;
}

export async function hasWorkspaceProjects(): Promise<boolean> {
    return (await discoverWorkspaceProjects()).length > 0;
}

export function getSelectedProjectUri(context: vscode.ExtensionContext): vscode.Uri | undefined {
    const storedId = context.workspaceState.get<string>(ZR_SELECTED_PROJECT_KEY);
    return storedId ? vscode.Uri.parse(storedId) : undefined;
}

export async function findProjectFile(folder: vscode.WorkspaceFolder | undefined): Promise<vscode.Uri | undefined> {
    const activeUri = vscode.window.activeTextEditor?.document.uri;
    if (activeUri && activeUri.scheme === 'file' && isZrpUri(activeUri)) {
        return activeUri;
    }

    const searchRoots = folder ? [folder] : (vscode.workspace.workspaceFolders ?? []);
    const candidates: vscode.Uri[] = [];

    for (const searchRoot of searchRoots) {
        const found = await vscode.workspace.findFiles(
            new vscode.RelativePattern(searchRoot, '**/*.zrp'),
            undefined,
            50,
        );
        candidates.push(...found);
    }

    if (candidates.length === 1) {
        return candidates[0];
    }
    if (candidates.length > 1) {
        const picked = await vscode.window.showQuickPick(
            candidates.map((uri) => ({
                label: lastPathSegment(uri.path),
                description: uri.fsPath,
                uri,
            })),
            {
                title: 'Select ZR project',
            },
        );
        return picked?.uri;
    }

    return undefined;
}

export function activeWorkspaceFolder(): vscode.WorkspaceFolder | undefined {
    const activeDocument = vscode.window.activeTextEditor?.document.uri;
    return activeDocument
        ? vscode.workspace.getWorkspaceFolder(activeDocument)
        : vscode.workspace.workspaceFolders?.[0];
}

export function resolveRelativePath(folder: vscode.WorkspaceFolder | undefined, value: string): string {
    const trimmed = value.trim();
    if (trimmed.length === 0) {
        return trimmed;
    }

    try {
        if (vscode.Uri.file(trimmed).fsPath === trimmed && /^[A-Za-z]:[\\/]/.test(trimmed)) {
            return vscode.Uri.file(trimmed).fsPath;
        }
    } catch {
        // Fall back to workspace-relative resolution below.
    }

    if (/^(\/|[A-Za-z]:[\\/])/.test(trimmed)) {
        return vscode.Uri.file(trimmed).fsPath;
    }

    const base = folder?.uri ?? vscode.workspace.workspaceFolders?.[0]?.uri;
    if (!base) {
        return trimmed;
    }
    return vscode.Uri.joinPath(base, trimmed).fsPath;
}

function relativePathWithinFolder(workspaceFolder: vscode.WorkspaceFolder, uri: vscode.Uri): string {
    const folderPath = normalizeFilePath(workspaceFolder.uri.path);
    const filePath = normalizeFilePath(uri.path);
    if (filePath.startsWith(`${folderPath}/`)) {
        return filePath.slice(folderPath.length + 1);
    }

    return vscode.workspace.asRelativePath(uri, false);
}

function removeExtension(value: string): string {
    const lastDot = value.lastIndexOf('.');
    return lastDot > 0 ? value.slice(0, lastDot) : value;
}

function lastPathSegment(pathValue: string): string {
    const normalized = normalizeFilePath(pathValue);
    const segments = normalized.split('/').filter((segment) => segment.length > 0);
    return segments[segments.length - 1] ?? normalized;
}

function compareStrings(left: string, right: string): number {
    return left.localeCompare(right);
}

async function setSelectedProjectUri(
    context: vscode.ExtensionContext,
    uri: vscode.Uri | undefined,
): Promise<void> {
    const nextValue = uri?.toString();
    const previousValue = context.workspaceState.get<string>(ZR_SELECTED_PROJECT_KEY);

    if (previousValue === nextValue) {
        return;
    }

    await context.workspaceState.update(ZR_SELECTED_PROJECT_KEY, nextValue);
    onDidChangeSelectedProjectEmitter.fire(uri);
}
