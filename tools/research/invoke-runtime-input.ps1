[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$MailboxDirectory,
    [Parameter(Mandatory = $true)]
    [string]$RunId,
    [string]$EvidencePath,
    [Parameter(Mandatory = $true)]
    [ValidateSet(
        'menu_up', 'nav_down', 'nav_left', 'nav_right', 'accept', 'pause',
        'probe_escape', 'show_probe', 'hide_probe', 'inject_pause_entry',
        'probe_pause_root', 'probe_main_root')]
    [string]$Command,
    [uint32]$CommandId = 0,
    [ValidateRange(1, 60)]
    [int]$TimeoutSeconds = 15
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$resolvedDirectory = (Resolve-Path -LiteralPath $MailboxDirectory).Path
if ([string]::IsNullOrWhiteSpace($EvidencePath)) {
    $evidencePath = Join-Path $resolvedDirectory 'AbsoluteControlPanel.evidence.jsonl'
} else {
    $evidencePath = (Resolve-Path -LiteralPath $EvidencePath).Path
}
$inputPath = Join-Path $resolvedDirectory (
    "AbsoluteControlPanelResearch.$RunId.input")

if ($CommandId -eq 0) {
    $maximumId = 0
    if (Test-Path -LiteralPath $evidencePath -PathType Leaf) {
        foreach ($line in Get-Content -LiteralPath $evidencePath -Tail 5000) {
            if ($line -notmatch '"run_id":"([^\"]+)"' -or $Matches[1] -ne $RunId) {
                continue
            }
            if ($line -match '"detail":"id=(\d+)') {
                $maximumId = [Math]::Max($maximumId, [uint32]$Matches[1])
            }
        }
    }
    $CommandId = $maximumId + 1
}

$temporaryPath = "$inputPath.tmp"
[System.IO.File]::WriteAllLines(
    $temporaryPath,
    [string[]]@("id=$CommandId", "command=$Command"),
    [System.Text.UTF8Encoding]::new($false))
Move-Item -LiteralPath $temporaryPath -Destination $inputPath -Force

$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$accepted = $null
$completed = $null
while ([DateTime]::UtcNow -lt $deadline) {
    if (Test-Path -LiteralPath $evidencePath -PathType Leaf) {
        $records = Get-Content -LiteralPath $evidencePath -Tail 250 |
            ForEach-Object {
                try { $_ | ConvertFrom-Json } catch { $null }
            } | Where-Object {
                $null -ne $_ -and $_.run_id -eq $RunId -and
                $_.detail -like "id=$CommandId*"
            }
        $accepted = $records | Where-Object {
            $_.event -in @(
                'research_input_queued', 'research_probe_command_queued',
                'pause_entry_injection_queued', 'foreign_menu_probe_queued')
        } | Select-Object -Last 1
        $completed = $records | Where-Object {
            $_.event -in @(
                'research_input_key_up', 'research_probe_command_dispatched',
                'pause_entry_injection_queued', 'foreign_menu_probe_queued')
        } | Select-Object -Last 1
        if ($null -ne $accepted -and $null -ne $completed) {
            break
        }
    }
    Start-Sleep -Milliseconds 100
}

if ($null -eq $accepted -or $null -eq $completed) {
    throw "Runtime input timed out: id=$CommandId command=$Command"
}
if ($completed.event -eq 'research_input_key_up' -and
    $completed.detail -notlike '*sent=1 error=0*') {
    throw "Runtime input failed: $($completed.detail)"
}

[pscustomobject]@{
    command_id = $CommandId
    command = $Command
    accepted_event = $accepted.event
    completed_event = $completed.event
    completed_detail = $completed.detail
}
