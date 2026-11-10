import {execFile} from 'node:child_process';
import * as path from 'node:path';
import {promisify} from 'node:util';

export type Kind = 'application' | 'library';
export type PromptState = 'unseen' | 'offered' | 'accepted' | 'declined' | 'dismissed';
interface Root { root: string; rootIdentity: string; }
export interface Inventory extends Root {
    complete: boolean;
    manifestPresent: boolean;
    eligibleCount: number;
    sources: string[];
}
export interface Plan extends Root {
    manifest: string;
    kind: Kind;
    name: string;
    version: string;
    projectId: string;
    roleId: string;
    packageName: string;
}
async function request(server: string, args: string[]): Promise<unknown> {
    try {
        const {stdout} = await promisify(execFile)(server, args, {encoding: 'utf8', maxBuffer: 65536, timeout: 10000, windowsHide: true});
        return JSON.parse(stdout);
    } catch (error) {
        throw new Error(`Project creation requires a matching voidls and an admitted directory. ${String(error)}`);
    }
}
function rootValid<T extends Partial<Root>>(root: T): root is T & Root {
    return typeof root.root === 'string' && path.isAbsolute(root.root) &&
        typeof root.rootIdentity === 'string' && /^[0-9]+(?::[0-9]+)+$/.test(root.rootIdentity);
}
export async function probeCreation(server: string, workspace: string): Promise<Inventory> {
    const envelope = await request(server, ['--discover-creation', workspace, '']) as {schema?: number; creation?: Partial<Inventory>};
    const result = envelope?.creation;
    if (envelope?.schema !== 1 || !result || !rootValid(result) || path.relative(workspace, result.root) !== '' || typeof result.complete !== 'boolean' ||
        typeof result.manifestPresent !== 'boolean' || !Number.isSafeInteger(result.eligibleCount) ||
        result.eligibleCount! < 0 || !Array.isArray(result.sources) || result.sources.length > 16 ||
        result.sources.some(id => typeof id !== 'string' || !id.startsWith('/') || !id.endsWith('.void')) ||
        new Set(result.sources).size !== result.sources.length ||
        (result.complete && result.eligibleCount !== result.sources.length))
        throw new Error('Invalid compiler creation inventory.');
    return result as Inventory;
}
export function freshInventory(inventory: Inventory, paths: ReadonlySet<string>): boolean {
    if (inventory.manifestPresent || !inventory.complete || !inventory.sources.length) return false;
    const fresh = new Set(Array.from(paths, file => '/' + path.relative(inventory.root, file).split(path.sep).join('/')));
    return inventory.sources.every(id => fresh.has(id));
}
export function promptKey(root: Root): string {
    return 'void.creation.' + JSON.stringify([path.normalize(root.root), root.rootIdentity]);
}
export async function reviewPlan(server: string, root: Root, kind: Kind, name = '', packageName = ''): Promise<Plan|'collision'> {
    const plan = await request(server, ['--plan-project', root.root, kind, name, packageName]) as Partial<Plan> & {schema?: number; status?: string};
    if (!plan || plan.schema !== 1 || !rootValid(plan) || plan.root !== root.root || plan.rootIdentity !== root.rootIdentity)
        throw new Error('Project directory changed after review.');
    if (plan.status === 'collision') return 'collision';
    if (plan.kind !== kind || typeof plan.manifest !== 'string' || path.normalize(plan.manifest) !== path.join(root.root, 'void.project.toml') ||
        [plan.name, plan.version, plan.projectId, plan.roleId, plan.packageName].some(value => typeof value !== 'string'))
        throw new Error('Project creation review changed or returned an invalid plan.');
    return plan as Plan;
}
export async function createReviewed(server: string, plan: Plan): Promise<'created'|'collision'> {
    const reply = await request(server, ['--create-project', plan.root, plan.rootIdentity, plan.kind, plan.name, plan.version,
        plan.projectId, plan.roleId, plan.packageName]) as {schema?: number; root?: string; status?: 'created'|'collision'};
    if (reply?.schema !== 1 || reply.root !== plan.root || (reply.status !== 'created' && reply.status !== 'collision'))
        throw new Error('Invalid project creation result. Check the reviewed directory before retrying.');
    return reply.status;
}
