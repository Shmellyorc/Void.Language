# Read-only console-state observer around an unchanged native probe.
# Run directly in an interactive native developer PowerShell; never pipe it.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [string[]]$Arguments = @(),
    [Parameter(Mandatory=$true)][string]$Evidence,
    [Parameter(Mandatory=$true)][ValidateSet('Debug','Release')][string]$Configuration,
    [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-fA-F]{40}$')][string]$SourceCommit,
    [Parameter(Mandatory=$true)][ValidateSet('ClassicConsole','WindowsTerminal','ExplicitConPTY')][string]$HostKind,
    [Parameter(Mandatory=$true)][string]$HostDescription
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) { throw 'Native Windows required' }
if (-not ('VoidConsoleObservation' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public sealed class VoidConsoleState {
    public uint InputCodePage, OutputCodePage;
    public bool[] ConsoleHandles = new bool[3];
    public uint[] Modes = new uint[3];
    public int[] ModeErrors = new int[3];
}
public static class VoidConsoleObservation {
    [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr GetStdHandle(int n);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool GetConsoleMode(IntPtr h, out uint mode);
    [DllImport("kernel32.dll")] static extern uint GetConsoleCP();
    [DllImport("kernel32.dll")] static extern uint GetConsoleOutputCP();
    public static VoidConsoleState Read() {
        var s = new VoidConsoleState();
        for (int i=0; i<3; ++i) {
            s.ConsoleHandles[i] = GetConsoleMode(GetStdHandle(-10-i), out s.Modes[i]);
            s.ModeErrors[i] = s.ConsoleHandles[i] ? 0 : Marshal.GetLastWin32Error();
        }
        s.InputCodePage = GetConsoleCP(); s.OutputCodePage = GetConsoleOutputCP();
        return s;
    }
}
'@
}
$binary = (Resolve-Path -LiteralPath $Executable).Path
$destination = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Evidence)
if (Test-Path -LiteralPath $destination) { throw 'Choose a new evidence filename; existing records are preserved' }
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
$before = [VoidConsoleObservation]::Read()
$record = [ordered]@{
    observed_utc = [DateTime]::UtcNow.ToString('o')
    configuration = $Configuration
    source_commit_reported_by_operator = $SourceCommit
    executable = $binary
    executable_sha256 = (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant()
    arguments = $Arguments
    host_kind_reported_by_operator = $HostKind
    host_description = $HostDescription
    windows = [Environment]::OSVersion.VersionString
    powershell = $PSVersionTable.PSVersion.ToString()
    WT_SESSION = [Environment]::GetEnvironmentVariable('WT_SESSION')
    before = $before
    after = $null
    child_exit_code = $null
    observer_error = $null
    modes_and_code_pages_equal = $false
    console_acceptance = 'UNVERIFIED; this record observes parent console state only'
}
function Save-Observation { $record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $destination -Encoding UTF8 }
Save-Observation
try {
    $LASTEXITCODE = $null
    if (@($before.ConsoleHandles | Where-Object { -not $_ }).Count -ne 0 -or
        $before.InputCodePage -eq 0 -or $before.OutputCodePage -eq 0) {
        throw 'All three standard handles must be console handles; no redirected interactive pass'
    }
    # Direct invocation: no pipeline, capture variable, redirection or hidden console.
    & $binary @Arguments
    $record.child_exit_code = $LASTEXITCODE
} catch {
    if ($null -ne $LASTEXITCODE) { $record.child_exit_code = $LASTEXITCODE }
    $record.observer_error = $_.Exception.Message
} finally {
    $record.after = [VoidConsoleObservation]::Read()
    $after = $record.after
    $record.modes_and_code_pages_equal = (
        @($after.ConsoleHandles | Where-Object { -not $_ }).Count -eq 0 -and
        ($before.Modes -join ',') -eq ($after.Modes -join ',') -and
        $before.InputCodePage -eq $after.InputCodePage -and
        $before.OutputCodePage -eq $after.OutputCodePage)
    Save-Observation
}
if ($null -ne $record.observer_error) { throw $record.observer_error }
if (-not $record.modes_and_code_pages_equal) { throw 'Console mode/code-page mismatch; preserve evidence and stop' }
if ($null -eq $record.child_exit_code) { throw 'Missing native child exit code' }
if ($record.child_exit_code -ne 0) { exit $record.child_exit_code }
