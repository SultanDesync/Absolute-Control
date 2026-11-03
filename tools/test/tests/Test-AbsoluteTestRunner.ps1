[CmdletBinding()]
param()

# Offline self-test for tools\test: everything except launching Starfield.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot '..\AbsoluteTest.psm1') -Force
Import-AbsoluteTestWindow

$script:Failures = 0
function Check {
    param([bool]$Condition, [string]$Description)
    if ($Condition) { Write-Host "  ok   $Description" }
    else { Write-Host "  FAIL $Description" -ForegroundColor Red; $script:Failures++ }
}
function Throws {
    param([scriptblock]$Block)
    try { & $Block; return $false } catch { return $true }
}

$root = Join-Path ([System.IO.Path]::GetTempPath()) ("absolute-test-selftest-" + [Guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory -Path $root | Out-Null
$utf8 = [System.Text.UTF8Encoding]::new($false)
try {
    Write-Host 'Suites'
    $suitePath = Join-Path $PSScriptRoot '..\suites\menu-regression.json'
    $suite = Read-TestSuite $suitePath
    Check ($suite.name -eq 'menu-regression') 'menu-regression suite parses'
    Check (@($suite.tests | Where-Object { $_.id -eq 'menu-open-close' })[0].repeat -eq 25) 'open/close test repeats 25 times'
    $bad = Join-Path $root 'bad.json'
    [System.IO.File]::WriteAllText($bad, '{"name":"x","tests":[{"id":"a","steps":[{"command":"pause","expect":"y"}]}]}', $utf8)
    Check (Throws { Read-TestSuite $bad }) 'a step with two kinds is rejected'
    [System.IO.File]::WriteAllText($bad, '{"name":"x","tests":[{"id":"a","steps":[]},{"id":"a","steps":[]}]}', $utf8)
    Check (Throws { Read-TestSuite $bad }) 'duplicate test ids are rejected'

    Write-Host 'INI rewriting'
    $ini = Set-IniValues -Lines @('; header', '[ControlPanel]', 'RunId=old', 'EnableMainMenuEntry=true', '[Other]', 'RunId=keep') `
        -Section 'ControlPanel' -Values @{ RunId = 'new'; AdvanceTitleWithSendInput = 'true' }
    Check ($ini -contains 'RunId=new') 'existing key in the section is replaced'
    Check ($ini -contains 'AdvanceTitleWithSendInput=true') 'missing key is added to the section'
    Check ($ini -contains 'EnableMainMenuEntry=true' -and $ini -contains 'RunId=keep' -and $ini[0] -eq '; header') 'other keys, sections and comments are preserved'
    Check ([Array]::IndexOf($ini, 'AdvanceTitleWithSendInput=true') -lt [Array]::IndexOf($ini, '[Other]')) 'added key stays inside its section'

    Write-Host 'MO2 profile'
    $base = Join-Path $root 'mo2'
    $profileDir = Join-Path $base 'profiles\Test Profile'
    New-Item -ItemType Directory -Force -Path $profileDir | Out-Null
    $modList = Join-Path $profileDir 'modlist.txt'
    [System.IO.File]::WriteAllText($modList, ("# generated`r`n+AbsoluteHOTAS 5.2.1`r`n+Absolute Control 1.0.0-dev rewrite`r`n" +
        "-Absolute Control test-hooks`r`n+SFSE`r`n*Unmanaged: X`r`n"), $utf8)
    $config = [pscustomobject]@{ mo2Executable = 'x'; mo2BaseDirectory = $base; profile = 'Test Profile'; mo2Shortcut = 'sfse_loader'
        testModName = 'Absolute Control test-hooks'; processName = 'Starfield'; sfseLogDirectory = $root; gameDirectory = ''
        artifactRoot = ''; conflictingModPatterns = @('Absolute Control*', 'AbsoluteControlPanel*'); killOnTimeout = $true
        launchTimeoutSeconds = 1; titleTimeoutSeconds = 1; loadTimeoutSeconds = 1; shutdownTimeoutSeconds = 1; commandTimeoutSeconds = 1 }
    $problems = @(Test-ProfileReadiness $config)
    Check ($problems.Count -eq 2) "disabled test mod and enabled rewrite host are both reported ($($problems.Count))"
    $changes = @(Set-ProfileForTesting -Config $config -BackupDirectory (Join-Path $root 'backup') -AssumeManagerClosed)
    Check ($changes.Count -eq 2) "profile preparation makes two changes ($($changes -join '; '))"
    Check (@(Test-ProfileReadiness $config).Count -eq 0) 'profile is ready afterwards'
    $after = [System.IO.File]::ReadAllText($modList)
    Check ($after.Contains("+AbsoluteHOTAS 5.2.1`r`n-Absolute Control 1.0.0-dev rewrite`r`n+Absolute Control test-hooks`r`n")) 'CRLF and mod order are preserved'
    Check (Test-Path (Join-Path $root 'backup\modlist.before-test.txt')) 'mod list is backed up first'

    Write-Host 'Deploy'
    $repo = Join-Path $root 'repo'
    foreach ($file in @('build\test-hooks\releasedbg\AbsoluteControlPanel.dll', 'build\test-hooks\releasedbg\AbsoluteControlPanel.pdb',
            'interface\dist\AbsoluteControlPanelMenu.swf')) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent (Join-Path $repo $file)) | Out-Null
        [System.IO.File]::WriteAllText((Join-Path $repo $file), $file, $utf8)
    }
    New-Item -ItemType Directory -Force -Path (Join-Path $repo 'config') | Out-Null
    [System.IO.File]::WriteAllLines((Join-Path $repo 'config\AbsoluteControlPanel.ini'), [string[]]@('[ControlPanel]', 'RunId=baseline', 'AutoOpen=false'), $utf8)
    $deployed = Publish-TestHooksBuild -Config $config -RepoRoot $repo -RunId 'run-42'
    $deployedIni = [System.IO.File]::ReadAllLines((Join-Path (Get-TestModDirectory $config) 'SFSE\Plugins\AbsoluteControlPanel.ini'))
    Check ($deployedIni -contains 'RunId=run-42' -and $deployedIni -contains 'AdvanceTitleWithSendInput=true') 'deployed INI carries the run id and title advance'
    Check ($deployed.Count -eq 4) 'DLL, PDB, SWF and INI are deployed and hashed'
    Check (Test-Path (Join-Path (Get-TestModDirectory $config) 'meta.ini')) 'meta.ini is created for MO2'

    Write-Host 'Mailbox'
    $mailbox = Join-Path $root 'AbsoluteControlPanelResearch.run-42.input'
    Write-MailboxCommand -Path $mailbox -Id 7 -Command 'open_page:absolute.hotas/hotas-throttle'
    $bytes = [System.IO.File]::ReadAllBytes($mailbox)
    Check ($bytes[0] -ne 0xEF) 'mailbox has no byte-order mark'
    Check (([System.IO.File]::ReadAllLines($mailbox) -join '|') -eq 'id=7|command=open_page:absolute.hotas/hotas-throttle') 'mailbox holds id and command lines'
    Check (-not (Test-Path "$mailbox.tmp")) 'temporary mailbox file is moved into place'

    Write-Host 'Evidence reader (real plugin records)'
    $evidence = Join-Path $root 'AbsoluteControlPanel.evidence.jsonl'
    $fixture = [System.IO.File]::ReadAllLines((Join-Path $PSScriptRoot 'fixtures\evidence-sample.jsonl'))
    $foreign = '{"timestamp_ms":1,"sequence":1,"run_id":"manual","level":"info","event":"menu_message_show","detail":"result=0"}'
    [System.IO.File]::WriteAllText($evidence, ($foreign + "`n" + ($fixture[0..4] -join "`n") + "`n" + $fixture[5].Substring(0, 30)), $utf8)
    $reader = New-EvidenceReader -Path $evidence -RunId 'fixture-run'
    Update-EvidenceReader $reader
    Check ($reader.Records.Count -eq 5) "other run ids and a half-written line are skipped ($($reader.Records.Count))"
    [System.IO.File]::AppendAllText($evidence, ($fixture[5].Substring(30) + "`n" + ($fixture[6..($fixture.Count - 1)] -join "`n") + "`n"), $utf8)
    Update-EvidenceReader $reader
    Check ($reader.Records.Count -eq $fixture.Count) "completed line and later lines are read incrementally ($($reader.Records.Count))"
    foreach ($expectation in @(
            @{ Event = 'main_menu_entry_injected'; Detail = @('label=ABSOLUTE CONTROL', 'invoked=true') },
            @{ Event = 'pause_entry_injected'; Detail = @('label=ABSOLUTE CONTROL', 'invoked=true') },
            @{ Event = 'menu_message_show'; Detail = @('result=0') },
            @{ Event = 'bridge_model_published'; Detail = @('accepted=true') },
            @{ Event = 'menu_message_hide'; Detail = @('result=0') },
            @{ Event = 'bridge_page_applied'; Detail = @('page=hotas-flight-axes controls=', 'active=true') },
            @{ Event = 'bridge_page_applied'; Detail = @('page=hotas-throttle controls=', 'active=true') })) {
        $found = Find-EvidenceRecord -Reader $reader -Event $expectation.Event -DetailContains $expectation.Detail
        Check ($null -ne $found) "suite expectation matches real output: $($expectation.Event) [$($expectation.Detail -join ' + ')]"
    }
    $hide = Find-EvidenceRecord -Reader $reader -Event 'menu_message_hide'
    Check ($null -eq (Find-EvidenceRecord -Reader $reader -Event 'menu_message_show' -AfterIndex $hide.Index)) 'AfterIndex excludes earlier records'
    Check ($null -eq (Find-EvidenceRecord -Reader $reader -Event 'bridge_page_applied' -DetailContains @('page=hotas-flight-axes', 'active=false'))) 'all detail parts must match'
    [System.IO.File]::WriteAllText($evidence, ($fixture[2] + "`n"), $utf8)
    Update-EvidenceReader $reader
    Check ($reader.Offset -lt 1000 -and $reader.Records.Count -eq $fixture.Count + 1) 'a rotated (shorter) log restarts from the beginning'

    Write-Host 'Crash detection and the step engine'
    $context = New-TestContext -Config $config -RunId 'fixture-run' -RunDirectory $root
    $context.Evidence = New-EvidenceReader -Path $evidence -RunId 'fixture-run'
    [System.IO.File]::WriteAllText($evidence, (($fixture -join "`n") + "`n"), $utf8)
    Check ($null -eq (Get-CrashSignal $context)) 'no crash without new dumps or a process'
    $passing = [pscustomobject]@{ name = 'fake'; tests = @(
        [pscustomobject]@{ id = 'finds-show'; steps = @([pscustomobject]@{ expect = 'menu_message_show'; detail = 'result=0'; since = 'run' }) },
        [pscustomobject]@{ id = 'times-out'; steps = @([pscustomobject]@{ expect = 'never_emitted'; timeoutSeconds = 1 }) },
        [pscustomobject]@{ id = 'waits'; steps = @([pscustomobject]@{ waitMs = 200 }) }) }
    $results = @(Invoke-TestSuite -Context $context -Suite $passing)
    Check ($results[0].status -eq 'passed') 'an expectation present in the evidence passes'
    Check ($results[1].status -eq 'failed' -and $results[1].message -like '*Timed out*') 'a missing event fails with a timeout'
    Check ($results[2].status -eq 'passed') 'a wait step passes'
    $dumpDir = Join-Path $base 'overwrite\Root'
    New-Item -ItemType Directory -Force -Path $dumpDir | Out-Null
    $context.DumpBaseline = Get-DumpSnapshot $context
    $crashing = [pscustomobject]@{ name = 'fake'; tests = @(
        [pscustomobject]@{ id = 'crashes'; steps = @([pscustomobject]@{ waitMs = 3000 }) },
        [pscustomobject]@{ id = 'after-crash'; steps = @([pscustomobject]@{ waitMs = 10 }) }) }
    $job = Start-Job -ScriptBlock { param($path) Start-Sleep -Milliseconds 600; [System.IO.File]::WriteAllText($path, 'dump') } `
        -ArgumentList (Join-Path $dumpDir 'Starfield_test.dmp')
    $results = @(Invoke-TestSuite -Context $context -Suite $crashing)
    Wait-Job $job | Remove-Job
    Check ($results[0].status -eq 'crashed' -and $results[0].message -like '*crash dump written*') 'a new dump in overwrite\Root is reported as a crash'
    Check ($results[1].status -eq 'not_run') 'tests after a crash are not run'
    $moved = @(Save-RunArtifacts $context)
    Check ($moved.Count -eq 1 -and -not (Test-Path (Join-Path $dumpDir 'Starfield_test.dmp'))) 'the dump is moved out of overwrite into the run folder'
    $summary = Write-TestReport -Context $context -Suite $crashing -Results $results -Shutdown 'not_started' -Deployed $deployed -Dumps $moved -FatalError ''
    Check ($summary.failed -eq 1 -and $summary.notRun -eq 1) 'the report counts failures and not-run tests'
    Check ((Test-Path (Join-Path $root 'report.md')) -and (Test-Path (Join-Path $root 'result.json'))) 'report.md and result.json are written'
} finally {
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host ''
if ($script:Failures -gt 0) { Write-Host "$script:Failures check(s) failed" -ForegroundColor Red; exit 1 }
Write-Host 'All runner self-test checks passed' -ForegroundColor Green
exit 0
