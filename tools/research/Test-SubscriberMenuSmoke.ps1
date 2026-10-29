[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$EvidencePath,
    [Parameter(Mandatory = $true)][string]$RunId,
    [ValidateSet('Zero', 'Power', 'HeadTracking')]
    [string[]]$Subscribers = @('Zero', 'Power', 'HeadTracking'),
    [string]$ReportPath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$expected = @{
    Zero = @('absolute.zero/mouse-alignment', 'absolute.zero/mouse-alignment-bindings')
    Power = @('absolute.power/power-presets', 'absolute.power/power-automation', 'absolute.power/power-diagnostics')
    HeadTracking = @('absolute.head_tracking/axes', 'absolute.head_tracking/general',
        'absolute.head_tracking/aim-restriction', 'absolute.head_tracking/bindings')
}
$required = @($Subscribers | ForEach-Object { $expected[$_] })
$events = @(Get-Content -LiteralPath $EvidencePath | Where-Object { $_.Trim() } |
    ForEach-Object { $_ | ConvertFrom-Json } | Where-Object { $_.run_id -ceq $RunId })
if ($events.Count -eq 0) { throw "No evidence for run '$RunId'." }

# Controls are populated only for the active page. Qualify each visited page
# against its own acknowledged publication within this exact run.
$verified = @{}
foreach ($publication in @($events | Where-Object {
    $_.event -ceq 'bridge_model_published' -and
    $_.detail -match 'populated=true invoked=true accepted=true$'
})) {
    if ($publication.detail -notmatch '^generation=(\d+) ') { continue }
    $generation = $Matches[1]
    if (-not @($events | Where-Object {
        $_.event -ceq 'bridge_model_applied' -and $_.detail -ceq "generation=$generation"
    }).Count) { continue }
    $pages = @($events | Where-Object { $_.event -ceq 'bridge_page_applied' } |
        ForEach-Object {
            if ($_.detail -match '^generation=(\d+) module=(\S+) page=(\S+) controls=(\d+) active=(true|false)$' -and
                $Matches[1] -ceq $generation -and $Matches[5] -ceq 'true') {
                [pscustomobject]@{ page = "$($Matches[2])/$($Matches[3])"; controls = [int]$Matches[4]; generation = $generation }
            }
        })
    if ($pages.Count -eq 1 -and $pages[0].controls -gt 0 -and
        $required -ccontains $pages[0].page) {
        $verified[$pages[0].page] = $pages[0]
    }
}
$missing = @($required | Where-Object { -not $verified.ContainsKey($_) })
if ($missing.Count -gt 0) {
    throw "Subscriber pages not verified as active with controls in an accepted model: $($missing -join ', ')"
}
$passing = [ordered]@{
    runId = $RunId
    status = 'passed'
    subscribers = $Subscribers
    pages = @($required | ForEach-Object { $verified[$_] })
    claim = 'Each subscriber page opened with controls in a movie-acknowledged AC menu model.'
    gameplayVerified = $false
    nativeBindingsVerified = $false
}
$json = $passing | ConvertTo-Json -Depth 5
if ($ReportPath) {
    [IO.File]::WriteAllText([IO.Path]::GetFullPath($ReportPath), $json, [Text.UTF8Encoding]::new($false))
}
$json
