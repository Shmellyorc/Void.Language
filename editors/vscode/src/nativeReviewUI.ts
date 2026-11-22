import * as vscode from 'vscode';
import type {NativeReview} from './nativeApproval';

// QuickPick is an established, disposable editor control. A task's close or
// compiler timeout dismisses it; dismissing it returns denial, never consent.
export function nativeReviewUI(request: NativeReview): {decision: Promise<boolean>; dispose(): void} {
    const picker = vscode.window.createQuickPick<vscode.QuickPickItem & {decision?: boolean}>();
    picker.title = 'Allow native code for this VOID application?';
    picker.placeholder = 'Native code runs with your user privileges. Review the exact identity before approving.';
    picker.ignoreFocusOut = true;
    const deny = {label: 'Deny', detail: 'Stop this workflow without saving approval.', decision: false};
    picker.items = [
        {label: 'Verified provider', detail: request.provider},
        {label: 'Approval ID (SHA-256)', detail: request.id},
        deny,
        {label: 'Approve Native Code', detail: 'Save consent for this exact application and provider identity.', decision: true},
    ];
    picker.activeItems = [deny];
    let settle: (value: boolean) => void;
    const decision = new Promise<boolean>(resolve => {settle = resolve;});
    const close = (approved: boolean) => {settle(approved); picker.hide(); picker.dispose();};
    picker.onDidAccept(() => {
        const selected = picker.selectedItems[0];
        if (selected?.decision !== undefined) close(selected.decision);
    });
    picker.onDidHide(() => {settle(false); picker.dispose();});
    picker.show();
    return {decision, dispose: () => close(false)};
}
