$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$researchRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$fixtureDirectory = Join-Path $researchRoot '../../artifacts/subscriber-smoke-fixtures'
New-Item -ItemType Directory -Force -Path $fixtureDirectory | Out-Null
$fixturePath = Join-Path $fixtureDirectory 'events.jsonl'

function Event([string]$Name, [string]$Detail, [string]$Run = 'fixture') {
    @{ run_id = $Run; event = $Name; detail = $Detail } | ConvertTo-Json -Compress
}
function Verify([string]$Name, [string[]]$Lines, [bool]$ShouldPass) {
    [IO.File]::WriteAllLines([IO.Path]::GetFullPath($fixturePath), $Lines)
    $passed = $false
    try {
        $report = & (Join-Path $researchRoot 'Test-SubscriberMenuSmoke.ps1') `
            -EvidencePath $fixturePath -RunId fixture -Subscribers Zero | ConvertFrom-Json
        $passed = $report.status -ceq 'passed' -and $report.pages.Count -eq 2
    } catch {
        if ($ShouldPass) { throw }
    }
    if ($passed -ne $ShouldPass) { throw "Unexpected smoke verdict: $Name" }
}
$ack = Event 'bridge_model_applied' 'generation=7'
$published = Event 'bridge_model_published' 'generation=7 revision=1 modules=1 pages=2 populated=true invoked=true accepted=true'
$landing = Event 'bridge_page_applied' 'generation=7 module=absolute.zero page=mouse-alignment controls=8 active=true'
$bindings = Event 'bridge_page_applied' 'generation=8 module=absolute.zero page=mouse-alignment-bindings controls=3 active=true'
$second = @($ack.Replace('generation=7', 'generation=8'), $published.Replace('generation=7', 'generation=8'))
Verify 'complete visits' (@($ack, $published, $landing, $bindings) + $second) $true
Verify 'missing page' @($ack, $published, $landing) $false
Verify 'missing acknowledgement' @($published, $landing, $bindings) $false
Verify 'duplicate inventory' (@($ack, $published, $landing, $bindings, $bindings) + $second) $false
Verify 'mixed generations' (@($ack, $published, $landing, $bindings.Replace('generation=8', 'generation=9')) + $second) $false
Verify 'empty controls' (@($ack, $published, $landing, $bindings.Replace('controls=3', 'controls=0')) + $second) $false
Verify 'rejected publication' (@($ack, $published.Replace('accepted=true', 'accepted=false'), $landing, $bindings) + $second) $false
Verify 'stale run' @((@($ack, $published, $landing, $bindings) + $second) | ForEach-Object { $_.Replace('fixture', 'old-run') }) $false
Verify 'inactive page' (@($ack, $published, $landing, $bindings.Replace('active=true', 'active=false')) + $second) $false
Write-Host 'Subscriber menu smoke verdicts passed (9 cases).'
