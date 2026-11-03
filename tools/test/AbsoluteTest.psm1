Set-StrictMode -Version Latest

# Runner module for automated in-game tests of the Absolute Control test-hooks build.
# The startup sequence, pixel heuristics and mailbox protocol are ported from the
# September research runner (Absolute-Control 03162e7 tools/research/run-probe.ps1).

$script:Utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$script:CompletionEvents = @('research_input_key_up', 'research_probe_command_dispatched')

function Import-AbsoluteTestWindow {
    if ('AbsoluteTestWindow' -as [type]) { return }
    Add-Type -AssemblyName System.Drawing
    Add-Type -Path (Join-Path $PSScriptRoot 'AbsoluteTestWindow.cs') -ReferencedAssemblies System.Drawing
    if (-not ('AbsoluteTestFocus' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class AbsoluteTestFocus
{
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
}
'@
    }
}

# ---------------------------------------------------------------- config

function Read-AbsoluteTestConfig {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Test config not found: $Path. Copy tools\test\test.local.example.json to test.local.json and edit it."
    }
    $config = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    foreach ($name in @('mo2Executable', 'mo2BaseDirectory', 'profile', 'mo2Shortcut', 'testModName', 'processName')) {
        if (-not ($config.PSObject.Properties.Name -contains $name) -or [string]::IsNullOrWhiteSpace([string]$config.$name)) {
            throw "Test config is missing '$name'."
        }
    }
    $defaults = [ordered]@{
        sfseLogDirectory = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'My Games\Starfield\SFSE\Logs'
        gameDirectory = ''
        artifactRoot = ''
        conflictingModPatterns = @('Absolute Control*', 'AbsoluteControlPanel*')
        killOnTimeout = $true
        launchTimeoutSeconds = 180
        titleTimeoutSeconds = 180
        loadTimeoutSeconds = 150
        shutdownTimeoutSeconds = 45
        commandTimeoutSeconds = 15
    }
    foreach ($key in $defaults.Keys) {
        if (-not ($config.PSObject.Properties.Name -contains $key) -or $null -eq $config.$key -or
            ($config.$key -is [string] -and [string]::IsNullOrWhiteSpace($config.$key))) {
            $config | Add-Member -NotePropertyName $key -NotePropertyValue $defaults[$key] -Force
        }
    }
    return $config
}

function Get-ProfileDirectory {
    param($Config)
    return Join-Path $Config.mo2BaseDirectory (Join-Path 'profiles' $Config.profile)
}

function Get-TestModDirectory {
    param($Config)
    return Join-Path $Config.mo2BaseDirectory (Join-Path 'mods' $Config.testModName)
}

# ---------------------------------------------------------------- MO2 profile

function Read-ModList {
    param([Parameter(Mandatory = $true)][string]$Path)
    $entries = New-Object System.Collections.Generic.List[object]
    foreach ($line in [System.IO.File]::ReadAllLines($Path)) {
        if ($line.Length -lt 2 -or ($line[0] -ne '+' -and $line[0] -ne '-')) { continue }
        $entries.Add([pscustomobject]@{ Name = $line.Substring(1); Enabled = ($line[0] -eq '+') })
    }
    return $entries.ToArray()
}

function Test-ProfileReadiness {
    param($Config)
    $problems = New-Object System.Collections.Generic.List[string]
    $profileDirectory = Get-ProfileDirectory $Config
    $modList = Join-Path $profileDirectory 'modlist.txt'
    if (-not (Test-Path -LiteralPath $modList -PathType Leaf)) {
        $problems.Add("MO2 profile not found: $profileDirectory")
        return $problems.ToArray()
    }
    $entries = Read-ModList $modList
    $test = @($entries | Where-Object { $_.Name -eq $Config.testModName })
    if ($test.Count -eq 0) { $problems.Add("Test mod '$($Config.testModName)' is not in the profile's mod list.") }
    elseif (-not $test[0].Enabled) { $problems.Add("Test mod '$($Config.testModName)' is disabled in the profile.") }
    foreach ($entry in $entries) {
        if (-not $entry.Enabled -or $entry.Name -eq $Config.testModName) { continue }
        foreach ($pattern in @($Config.conflictingModPatterns)) {
            if ($entry.Name -like $pattern) {
                $problems.Add("Conflicting Absolute Control host is enabled: '$($entry.Name)'.")
                break
            }
        }
    }
    return $problems.ToArray()
}

function Set-ProfileForTesting {
    # Enables the test mod at top priority and disables conflicting hosts. MO2 rewrites
    # profile files when it exits, so this refuses to run while MO2 is open.
    # AssumeManagerClosed exists only for the self-test's temporary profile; never use it on a real one.
    param($Config, [Parameter(Mandatory = $true)][string]$BackupDirectory, [switch]$AssumeManagerClosed)
    if (-not $AssumeManagerClosed -and (Get-Process -Name ModOrganizer -ErrorAction SilentlyContinue)) {
        throw 'Close Mod Organizer before preparing the test profile.'
    }
    $modList = Join-Path (Get-ProfileDirectory $Config) 'modlist.txt'
    New-Item -ItemType Directory -Force -Path $BackupDirectory | Out-Null
    Copy-Item -LiteralPath $modList -Destination (Join-Path $BackupDirectory 'modlist.before-test.txt') -Force
    $bytes = [System.IO.File]::ReadAllBytes($modList)
    $text = $script:Utf8NoBom.GetString($bytes)
    $newline = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
    $lines = New-Object System.Collections.Generic.List[string]
    $lines.AddRange([string[]]($text -split "`r?`n"))
    while ($lines.Count -gt 0 -and $lines[$lines.Count - 1] -eq '') { $lines.RemoveAt($lines.Count - 1) }
    $changes = New-Object System.Collections.Generic.List[string]
    $found = $false
    for ($index = 0; $index -lt $lines.Count; $index++) {
        $line = $lines[$index]
        if ($line.Length -lt 2 -or ($line[0] -ne '+' -and $line[0] -ne '-')) { continue }
        $name = $line.Substring(1)
        if ($name -eq $Config.testModName) {
            $found = $true
            if ($line[0] -eq '-') { $lines[$index] = "+$name"; $changes.Add("enabled $name") }
            continue
        }
        if ($line[0] -eq '+') {
            foreach ($pattern in @($Config.conflictingModPatterns)) {
                if ($name -like $pattern) { $lines[$index] = "-$name"; $changes.Add("disabled $name"); break }
            }
        }
    }
    if (-not $found) {
        $insertAt = 0
        if ($lines.Count -gt 0 -and $lines[0].StartsWith('#')) { $insertAt = 1 }
        $lines.Insert($insertAt, "+$($Config.testModName)")
        $changes.Add("added $($Config.testModName)")
    }
    [System.IO.File]::WriteAllText($modList, (($lines -join $newline) + $newline), $script:Utf8NoBom)
    return $changes.ToArray()
}

# ---------------------------------------------------------------- deploy

function Set-IniValues {
    param([string[]]$Lines, [string]$Section, [hashtable]$Values)
    $output = New-Object System.Collections.Generic.List[string]
    $remaining = @{} + $Values
    $current = ''
    $inserted = $false
    foreach ($line in $Lines) {
        $trimmed = $line.Trim()
        if ($trimmed.StartsWith('[') -and $trimmed.EndsWith(']')) {
            if ($current -eq $Section -and -not $inserted) {
                foreach ($key in @($remaining.Keys)) { $output.Add("$key=$($remaining[$key])") }
                $remaining.Clear(); $inserted = $true
            }
            $current = $trimmed.Substring(1, $trimmed.Length - 2)
            $output.Add($line); continue
        }
        if ($current -eq $Section -and $trimmed -match '^([^;=]+?)\s*=') {
            $key = $Matches[1].Trim()
            if ($remaining.ContainsKey($key)) { $output.Add("$key=$($remaining[$key])"); $remaining.Remove($key); continue }
        }
        $output.Add($line)
    }
    if ($remaining.Count -gt 0) {
        if ($current -ne $Section) { $output.Add("[$Section]") }
        foreach ($key in @($remaining.Keys)) { $output.Add("$key=$($remaining[$key])") }
    }
    return $output.ToArray()
}

function Get-Sha256 {
    param([Parameter(Mandatory = $true)][string]$LiteralPath)
    $stream = [System.IO.File]::OpenRead($LiteralPath)
    $digest = [System.Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($digest.ComputeHash($stream))).Replace('-', '') }
    finally { $digest.Dispose(); $stream.Dispose() }
}

function Publish-TestHooksBuild {
    param($Config, [Parameter(Mandatory = $true)][string]$RepoRoot, [Parameter(Mandatory = $true)][string]$RunId,
        [string]$Mode = 'releasedbg')
    $build = Join-Path $RepoRoot "build\test-hooks\$Mode"
    $sources = [ordered]@{
        'SFSE\Plugins\AbsoluteControlPanel.dll' = Join-Path $build 'AbsoluteControlPanel.dll'
        'SFSE\Plugins\AbsoluteControlPanel.pdb' = Join-Path $build 'AbsoluteControlPanel.pdb'
        'Interface\AbsoluteControlPanelMenu.swf' = Join-Path $RepoRoot 'interface\dist\AbsoluteControlPanelMenu.swf'
    }
    foreach ($source in $sources.Values) {
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
            throw "Missing build output: $source. Run: xmake build -y AbsoluteControlPanelTestHooks"
        }
    }
    $mod = Get-TestModDirectory $Config
    $deployed = [ordered]@{}
    foreach ($relative in $sources.Keys) {
        $target = Join-Path $mod $relative
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
        Copy-Item -LiteralPath $sources[$relative] -Destination $target -Force
        $deployed[$relative] = Get-Sha256 $target
    }
    $iniLines = [System.IO.File]::ReadAllLines((Join-Path $RepoRoot 'config\AbsoluteControlPanel.ini'))
    $iniLines = Set-IniValues -Lines $iniLines -Section 'ControlPanel' -Values @{
        RunId = $RunId; AutoOpen = 'false'; RequireArm = 'false'; AdvanceTitleWithSendInput = 'true'
    }
    $iniPath = Join-Path $mod 'SFSE\Plugins\AbsoluteControlPanel.ini'
    [System.IO.File]::WriteAllLines($iniPath, $iniLines, $script:Utf8NoBom)
    $deployed['SFSE\Plugins\AbsoluteControlPanel.ini'] = Get-Sha256 $iniPath
    $meta = Join-Path $mod 'meta.ini'
    if (-not (Test-Path -LiteralPath $meta)) {
        [System.IO.File]::WriteAllLines($meta, [string[]]@('[General]', 'gameName=Starfield',
            'comments=Absolute Control TEST HOOKS build, deployed by tools\test. Never use for play.'), $script:Utf8NoBom)
    }
    return $deployed
}

# ---------------------------------------------------------------- evidence

function New-EvidenceReader {
    param([Parameter(Mandatory = $true)][string]$Path, [Parameter(Mandatory = $true)][string]$RunId)
    return @{ Path = $Path; RunId = $RunId; Offset = [long]0; Pending = ''; Created = $null
        Records = (New-Object System.Collections.Generic.List[object]) }
}

function Update-EvidenceReader {
    # Incrementally reads new lines. The plugin rotates the file at startup, so a
    # shorter file or a new creation time restarts reading from the beginning.
    param([Parameter(Mandatory = $true)][hashtable]$Reader)
    if (-not (Test-Path -LiteralPath $Reader.Path -PathType Leaf)) { return }
    $info = Get-Item -LiteralPath $Reader.Path
    if ($info.Length -lt $Reader.Offset -or ($null -ne $Reader.Created -and $info.CreationTimeUtc -ne $Reader.Created)) {
        $Reader.Offset = 0; $Reader.Pending = ''
    }
    $Reader.Created = $info.CreationTimeUtc
    if ($info.Length -eq $Reader.Offset) { return }
    $share = [System.IO.FileShare]::ReadWrite -bor [System.IO.FileShare]::Delete
    $stream = [System.IO.FileStream]::new($Reader.Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, $share)
    try {
        [void]$stream.Seek($Reader.Offset, [System.IO.SeekOrigin]::Begin)
        $buffer = New-Object byte[] ($stream.Length - $Reader.Offset)
        $read = $stream.Read($buffer, 0, $buffer.Length)
        $Reader.Offset += $read
    } finally { $stream.Dispose() }
    $text = $Reader.Pending + $script:Utf8NoBom.GetString($buffer, 0, $read)
    $lines = $text -split "`n"
    $Reader.Pending = $lines[$lines.Count - 1]
    $marker = '"run_id":"' + $Reader.RunId + '"'
    for ($index = 0; $index -lt $lines.Count - 1; $index++) {
        $line = $lines[$index].TrimEnd("`r")
        if (-not $line.Contains($marker)) { continue }
        try { $Reader.Records.Add(($line | ConvertFrom-Json)) } catch { }
    }
}

function Find-EvidenceRecord {
    # Returns the first record after AfterIndex with this event whose detail contains every substring.
    param([Parameter(Mandatory = $true)][hashtable]$Reader, [Parameter(Mandatory = $true)][string]$Event,
        [string[]]$DetailContains = @(), [int]$AfterIndex = -1)
    for ($index = [Math]::Max(0, $AfterIndex + 1); $index -lt $Reader.Records.Count; $index++) {
        $record = $Reader.Records[$index]
        if ($record.event -ne $Event) { continue }
        $detail = [string]$record.detail
        $matched = $true
        foreach ($part in $DetailContains) {
            if ($part -and -not $detail.Contains($part)) { $matched = $false; break }
        }
        if ($matched) { return [pscustomobject]@{ Index = $index; Record = $record } }
    }
    return $null
}

# ---------------------------------------------------------------- session context

function New-TestContext {
    param($Config, [string]$RunId, [string]$RunDirectory)
    $logDirectory = [Environment]::ExpandEnvironmentVariables([string]$Config.sfseLogDirectory)
    $context = @{
        Config = $Config; RunId = $RunId; RunDirectory = $RunDirectory; Process = $null
        Evidence = New-EvidenceReader -Path (Join-Path $logDirectory 'AbsoluteControlPanel.evidence.jsonl') -RunId $RunId
        MailboxPath = Join-Path $logDirectory "AbsoluteControlPanelResearch.$RunId.input"
        AdvancePath = Join-Path $logDirectory "AbsoluteControlPanelResearch.$RunId.advance"
        NextCommandId = [uint32]0; CommandCursor = -1
        DumpBaseline = @{}; RunnerLog = Join-Path $RunDirectory 'runner-events.jsonl'
    }
    $context.DumpBaseline = Get-DumpSnapshot $context
    return $context
}

function Write-RunnerEvent {
    param([hashtable]$Context, [string]$Event, [string]$Detail = '')
    $record = [ordered]@{ timestamp = [DateTimeOffset]::UtcNow.ToString('o'); event = $Event; detail = $Detail }
    [System.IO.File]::AppendAllText($Context.RunnerLog, (($record | ConvertTo-Json -Compress) + "`n"), $script:Utf8NoBom)
    Write-Host ("[{0:HH:mm:ss}] {1} {2}" -f (Get-Date), $Event, $Detail)
}

function Get-DumpDirectories {
    param([hashtable]$Context)
    $directories = @(Join-Path $Context.Config.mo2BaseDirectory 'overwrite\Root')
    if ($Context.Config.gameDirectory) { $directories += [string]$Context.Config.gameDirectory }
    return $directories
}

function Get-DumpSnapshot {
    param([hashtable]$Context)
    $snapshot = @{}
    foreach ($directory in Get-DumpDirectories $Context) {
        if (-not (Test-Path -LiteralPath $directory)) { continue }
        foreach ($file in Get-ChildItem -LiteralPath $directory -Filter '*.dmp' -File -ErrorAction SilentlyContinue) {
            $snapshot[$file.FullName] = $file.LastWriteTimeUtc
        }
    }
    return $snapshot
}

function Get-NewDumps {
    param([hashtable]$Context)
    $current = Get-DumpSnapshot $Context
    return @($current.Keys | Where-Object { -not $Context.DumpBaseline.ContainsKey($_) -or
        $Context.DumpBaseline[$_] -ne $current[$_] })
}

function Get-CrashSignal {
    # Returns a description when the game has crashed or hung on a dialog, otherwise $null.
    param([hashtable]$Context)
    $dumps = @(Get-NewDumps $Context)
    if ($dumps.Count -gt 0) { return "crash dump written: $($dumps -join ', ')" }
    $process = $Context.Process
    if ($null -eq $process) { return $null }
    $process.Refresh()
    if ($process.HasExited) { return "Starfield exited (code $($process.ExitCode))" }
    $dialogs = @([AbsoluteTestWindow]::GetProcessWindows($process.Id) | Where-Object { $_.ClassName -eq '#32770' })
    if ($dialogs.Count -gt 0) { return "Starfield opened a dialog: $(($dialogs | ForEach-Object Title) -join '; ')" }
    return $null
}

function Wait-Until {
    param([hashtable]$Context, [scriptblock]$Condition, [int]$TimeoutSeconds, [string]$Description,
        [int]$PollMilliseconds = 100)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        $crash = Get-CrashSignal $Context
        if ($crash) { throw [System.InvalidOperationException]::new("CRASH while waiting for ${Description}: $crash") }
        Update-EvidenceReader $Context.Evidence
        $value = & $Condition
        if ($null -ne $value -and $value -ne $false) { return $value }
        Start-Sleep -Milliseconds $PollMilliseconds
    }
    throw [System.TimeoutException]::new("Timed out after ${TimeoutSeconds}s waiting for $Description.")
}

function Wait-Evidence {
    param([hashtable]$Context, [string]$Event, [string[]]$DetailContains = @(), [int]$AfterIndex = -1,
        [int]$TimeoutSeconds = 15)
    $wanted = @($DetailContains | Where-Object { $_ })
    $description = "evidence $Event" + $(if ($wanted.Count -gt 0) { " containing '$($wanted -join "' + '")'" } else { '' })
    return Wait-Until -Context $Context -TimeoutSeconds $TimeoutSeconds -Description $description -Condition {
        Find-EvidenceRecord -Reader $Context.Evidence -Event $Event -DetailContains $DetailContains -AfterIndex $AfterIndex
    }
}

# ---------------------------------------------------------------- game window and input

function Get-GameWindow {
    param([hashtable]$Context)
    $process = $Context.Process
    $process.Refresh()
    $windows = @([AbsoluteTestWindow]::GetProcessWindows($process.Id))
    $window = $windows | Where-Object { $_.Handle -eq $process.MainWindowHandle } | Select-Object -First 1
    if ($null -eq $window) { $window = $windows | Where-Object { $_.Title -eq 'Starfield' } | Select-Object -First 1 }
    return $window
}

function Save-GameScreenshot {
    param([hashtable]$Context, [Parameter(Mandatory = $true)][string]$Path)
    $window = Get-GameWindow $Context
    if ($null -eq $window) { return $false }
    $width = $window.Bounds.Right - $window.Bounds.Left
    $height = $window.Bounds.Bottom - $window.Bounds.Top
    if ($width -le 0 -or $height -le 0) { return $false }
    $bitmap = New-Object System.Drawing.Bitmap($width, $height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $deviceContext = $graphics.GetHdc()
        try { $captured = [AbsoluteTestWindow]::PrintWindow($window.Handle, $deviceContext, 2) }
        finally { $graphics.ReleaseHdc($deviceContext) }
        if (-not $captured) { return $false }
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
        return $true
    } finally { $graphics.Dispose(); $bitmap.Dispose() }
}

function Set-GameForeground {
    # The plugin's SendInput goes to the foreground window, so key commands need the game in front.
    param([hashtable]$Context)
    $window = Get-GameWindow $Context
    if ($null -eq $window) { return $false }
    if ([AbsoluteTestFocus]::GetForegroundWindow() -eq $window.Handle) { return $true }
    [void][AbsoluteTestFocus]::SetForegroundWindow($window.Handle)
    Start-Sleep -Milliseconds 250
    $focused = [AbsoluteTestFocus]::GetForegroundWindow() -eq $window.Handle
    Write-RunnerEvent $Context 'focus_restored' "focused=$focused"
    return $focused
}

function Write-MailboxCommand {
    param([Parameter(Mandatory = $true)][string]$Path, [uint32]$Id, [Parameter(Mandatory = $true)][string]$Command)
    $temporary = "$Path.tmp"
    [System.IO.File]::WriteAllLines($temporary, [string[]]@("id=$Id", "command=$Command"), $script:Utf8NoBom)
    Move-Item -LiteralPath $temporary -Destination $Path -Force
}

function Invoke-GameCommand {
    # Sends one mailbox command and waits for the plugin to complete or reject it.
    param([hashtable]$Context, [Parameter(Mandatory = $true)][string]$Command, [int]$TimeoutSeconds = 0)
    if ($TimeoutSeconds -le 0) { $TimeoutSeconds = [int]$Context.Config.commandTimeoutSeconds }
    $Context.NextCommandId = [uint32]($Context.NextCommandId + 1)
    $id = $Context.NextCommandId
    Update-EvidenceReader $Context.Evidence
    $Context.CommandCursor = $Context.Evidence.Records.Count - 1
    [void](Set-GameForeground $Context)
    Write-MailboxCommand -Path $Context.MailboxPath -Id $id -Command $Command
    $prefix = "id=$id command=$Command"
    $result = Wait-Until -Context $Context -TimeoutSeconds $TimeoutSeconds -Description "command $prefix" -Condition {
        for ($index = $Context.CommandCursor + 1; $index -lt $Context.Evidence.Records.Count; $index++) {
            $record = $Context.Evidence.Records[$index]
            if (-not ([string]$record.detail).StartsWith($prefix)) { continue }
            if ($record.event -eq 'research_input_rejected') { return $record }
            if ($script:CompletionEvents -contains $record.event) { return $record }
        }
        return $false
    }
    if ($result.event -eq 'research_input_rejected') { throw "Command rejected: $($result.detail)" }
    if ($result.event -eq 'research_input_key_up' -and -not ([string]$result.detail).Contains('sent=1 error=0')) {
        throw "Key press not delivered: $($result.detail)"
    }
    Write-RunnerEvent $Context 'command_completed' $prefix
    return $result
}

# ---------------------------------------------------------------- game lifecycle

function Start-GameSession {
    # Launches the profile through MO2, advances the title screen and continues the latest save.
    param([hashtable]$Context)
    $config = $Context.Config
    if (Get-Process -Name $config.processName -ErrorAction SilentlyContinue) { throw 'Starfield is already running.' }
    $arguments = @('-p', ('"{0}"' -f $config.profile), ('"moshortcut://:{0}"' -f $config.mo2Shortcut))
    Write-RunnerEvent $Context 'launch_requested' ("{0} {1}" -f $config.mo2Executable, ($arguments -join ' '))
    Start-Process -FilePath $config.mo2Executable -ArgumentList $arguments | Out-Null
    $Context.Process = Wait-Until -Context $Context -TimeoutSeconds $config.launchTimeoutSeconds `
        -Description 'Starfield process' -PollMilliseconds 500 -Condition {
            Get-Process -Name $config.processName -ErrorAction SilentlyContinue | Select-Object -First 1
        }
    Write-RunnerEvent $Context 'game_process_seen' "pid=$($Context.Process.Id)"
    [void](Wait-Until -Context $Context -TimeoutSeconds $config.launchTimeoutSeconds -Description 'game window' `
        -PollMilliseconds 500 -Condition { Get-GameWindow $Context })

    # Only the test-hooks build with this run's RunId answers the .advance file, so a
    # missing or stale deployment fails at the title Enter wait below.
    $probe = Join-Path $Context.RunDirectory 'title-probe.png'
    [void](Wait-Until -Context $Context -TimeoutSeconds $config.titleTimeoutSeconds -Description 'title prompt' `
        -PollMilliseconds 250 -Condition {
            if (-not (Save-GameScreenshot -Context $Context -Path $probe)) { return $false }
            [AbsoluteTestWindow]::IsTitlePromptSignal([AbsoluteTestWindow]::AnalyzeBrightFooter($probe))
        })
    Write-RunnerEvent $Context 'title_prompt_seen'
    [System.IO.File]::WriteAllText($Context.AdvancePath, [DateTimeOffset]::UtcNow.ToString('o'), $script:Utf8NoBom)
    $enter = Wait-Evidence -Context $Context -Event 'title_enter_key_up' -TimeoutSeconds 30
    if (-not ([string]$enter.Record.detail).Contains('sent=1 error=0')) { throw "Title Enter failed: $($enter.Record.detail)" }
    $script:AbsentSamples = 0
    [void](Wait-Until -Context $Context -TimeoutSeconds 60 -Description 'title prompt cleared' -PollMilliseconds 250 -Condition {
        if (-not (Save-GameScreenshot -Context $Context -Path $probe)) { return $false }
        if ([AbsoluteTestWindow]::IsTitlePromptSignal([AbsoluteTestWindow]::AnalyzeBrightFooter($probe))) {
            $script:AbsentSamples = 0; return $false
        }
        $script:AbsentSamples++
        return $script:AbsentSamples -ge 2
    })
    [void](Wait-Evidence -Context $Context -Event 'research_input_mailbox_registered' `
        -DetailContains (Split-Path -Leaf $Context.MailboxPath) -TimeoutSeconds $config.titleTimeoutSeconds)
    Write-RunnerEvent $Context 'mailbox_ready'

    [void](Invoke-GameCommand -Context $Context -Command 'menu_up')
    $selection = Join-Path $Context.RunDirectory 'continue-selection.png'
    [void](Wait-Until -Context $Context -TimeoutSeconds 10 -Description 'Continue row selected' -PollMilliseconds 250 -Condition {
        if (-not (Save-GameScreenshot -Context $Context -Path $selection)) { return $false }
        $continue = [AbsoluteTestWindow]::AnalyzeMeanLuminance($selection, 0.220, 0.363, 0.008, 0.009)
        $newGame = [AbsoluteTestWindow]::AnalyzeMeanLuminance($selection, 0.220, 0.397, 0.008, 0.009)
        $continue -ge 180 -and ($continue - $newGame) -ge 50
    })
    [void](Invoke-GameCommand -Context $Context -Command 'accept')
    [void](Invoke-GameCommand -Context $Context -Command 'confirm_continue')
    [void](Wait-Evidence -Context $Context -Event 'active_menu_insert_completed' -DetailContains 'name=HUDMenu ' `
        -AfterIndex $Context.CommandCursor -TimeoutSeconds $config.loadTimeoutSeconds)
    Write-RunnerEvent $Context 'save_loaded'
}

function Stop-GameSession {
    param([hashtable]$Context, [switch]$KeepGameRunning)
    foreach ($path in @($Context.AdvancePath, $Context.MailboxPath, "$($Context.MailboxPath).tmp")) {
        if ($path -and (Test-Path -LiteralPath $path)) { Remove-Item -LiteralPath $path -Force }
    }
    $process = $Context.Process
    if ($null -eq $process) { return 'not_started' }
    $process.Refresh()
    if ($process.HasExited) { return 'already_exited' }
    if ($KeepGameRunning) { Write-RunnerEvent $Context 'session_retained' "pid=$($process.Id)"; return 'retained' }
    Write-RunnerEvent $Context 'shutdown_requested' 'CloseMainWindow'
    [void]$process.CloseMainWindow()
    if ($process.WaitForExit([int]$Context.Config.shutdownTimeoutSeconds * 1000)) { return 'closed' }
    if (-not $Context.Config.killOnTimeout) {
        Write-RunnerEvent $Context 'shutdown_incomplete' 'killOnTimeout=false; Starfield left running'
        return 'left_running'
    }
    Write-RunnerEvent $Context 'shutdown_forced' "pid=$($process.Id)"
    Stop-Process -Id $process.Id -Force
    [void]$process.WaitForExit(15000)
    return 'killed'
}

# ---------------------------------------------------------------- suites

function Read-TestSuite {
    param([Parameter(Mandatory = $true)][string]$Path)
    $suite = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    if (-not $suite.name) { throw "Suite has no name: $Path" }
    $ids = @{}
    foreach ($test in @($suite.tests)) {
        if (-not $test.id) { throw "A test in $Path has no id." }
        if ($ids.ContainsKey($test.id)) { throw "Duplicate test id '$($test.id)' in $Path." }
        $ids[$test.id] = $true
        foreach ($step in @($test.steps)) {
            $kinds = @($step.PSObject.Properties.Name | Where-Object { $_ -in @('command', 'expect', 'waitMs', 'screenshot') })
            if ($kinds.Count -ne 1) { throw "Test '$($test.id)' has a step without exactly one of command/expect/waitMs/screenshot." }
        }
    }
    return $suite
}

function Invoke-TestStep {
    param([hashtable]$Context, $Step, [string]$Label)
    if ($Step.PSObject.Properties.Name -contains 'command') {
        [void](Invoke-GameCommand -Context $Context -Command ([string]$Step.command)); return
    }
    if ($Step.PSObject.Properties.Name -contains 'expect') {
        # detail is a substring or a list of substrings that must all appear. By default only
        # evidence after the latest command counts; "since": "run" searches the whole run.
        $detail = if ($Step.PSObject.Properties.Name -contains 'detail') { [string[]]@($Step.detail) } else { [string[]]@() }
        $timeout = if ($Step.PSObject.Properties.Name -contains 'timeoutSeconds') { [int]$Step.timeoutSeconds } else { 10 }
        $after = $Context.CommandCursor
        if ($Step.PSObject.Properties.Name -contains 'since' -and $Step.since -eq 'run') { $after = -1 }
        [void](Wait-Evidence -Context $Context -Event ([string]$Step.expect) -DetailContains $detail `
            -AfterIndex $after -TimeoutSeconds $timeout)
        return
    }
    if ($Step.PSObject.Properties.Name -contains 'waitMs') {
        $deadline = [DateTime]::UtcNow.AddMilliseconds([int]$Step.waitMs)
        while ([DateTime]::UtcNow -lt $deadline) {
            $crash = Get-CrashSignal $Context
            if ($crash) { throw [System.InvalidOperationException]::new("CRASH during wait: $crash") }
            Start-Sleep -Milliseconds 100
        }
        return
    }
    [void](Save-GameScreenshot -Context $Context -Path (Join-Path $Context.RunDirectory "$Label-$($Step.screenshot).png"))
}

function Invoke-TestSuite {
    param([hashtable]$Context, $Suite)
    $results = New-Object System.Collections.Generic.List[object]
    $aborted = $null
    foreach ($test in @($Suite.tests)) {
        $repeat = if ($test.PSObject.Properties.Name -contains 'repeat') { [int]$test.repeat } else { 1 }
        if ($aborted) {
            $results.Add([pscustomobject]@{ id = $test.id; status = 'not_run'; iterations = 0; seconds = 0; message = $aborted })
            continue
        }
        $watch = [System.Diagnostics.Stopwatch]::StartNew()
        $status = 'passed'; $message = ''; $iteration = 0; $stepIndex = 0
        Write-RunnerEvent $Context 'test_started' "$($test.id) repeat=$repeat"
        try {
            for ($iteration = 1; $iteration -le $repeat; $iteration++) {
                $stepIndex = 0
                foreach ($step in @($test.steps)) {
                    $stepIndex++
                    Invoke-TestStep -Context $Context -Step $step -Label ("{0}-{1}-{2}" -f $test.id, $iteration, $stepIndex)
                }
            }
            $iteration = $repeat
        } catch {
            $message = "iteration $iteration, step ${stepIndex}: $($_.Exception.Message)"
            if ($_.Exception -is [System.InvalidOperationException] -and $_.Exception.Message.StartsWith('CRASH')) {
                $status = 'crashed'; $aborted = "suite aborted after crash in $($test.id)"
            } else {
                $status = 'failed'
                try { [void](Save-GameScreenshot -Context $Context -Path (Join-Path $Context.RunDirectory "$($test.id)-failure.png")) } catch { }
            }
        }
        $watch.Stop()
        $results.Add([pscustomobject]@{ id = $test.id; status = $status; iterations = $iteration
            seconds = [Math]::Round($watch.Elapsed.TotalSeconds, 1); message = $message })
        Write-RunnerEvent $Context 'test_finished' "$($test.id) status=$status $message"
    }
    return $results.ToArray()
}

# ---------------------------------------------------------------- reporting

function Save-RunArtifacts {
    # Copies this run's plugin evidence and moves new crash dumps out of MO2's overwrite
    # (Root Builder would otherwise copy them into the game folder on the next launch).
    param([hashtable]$Context)
    Update-EvidenceReader $Context.Evidence
    $lines = @($Context.Evidence.Records | ForEach-Object { $_ | ConvertTo-Json -Compress })
    [System.IO.File]::WriteAllLines((Join-Path $Context.RunDirectory 'plugin-events.jsonl'), [string[]]$lines, $script:Utf8NoBom)
    $moved = @()
    foreach ($dump in Get-NewDumps $Context) {
        $target = Join-Path $Context.RunDirectory (Split-Path -Leaf $dump)
        Move-Item -LiteralPath $dump -Destination $target -Force
        $moved += $target
    }
    return $moved
}

function Write-TestReport {
    param([hashtable]$Context, $Suite, $Results, [string]$Shutdown, $Deployed, [string[]]$Dumps, [string]$FatalError)
    $summary = [ordered]@{
        suite = $Suite.name; runId = $Context.RunId; profile = $Context.Config.profile
        finished = [DateTimeOffset]::UtcNow.ToString('o'); shutdown = $Shutdown; fatalError = $FatalError
        passed = @($Results | Where-Object status -eq 'passed').Count
        failed = @($Results | Where-Object { $_.status -in @('failed', 'crashed') }).Count
        notRun = @($Results | Where-Object status -eq 'not_run').Count
        deployed = $Deployed; crashDumps = $Dumps; tests = $Results
    }
    [System.IO.File]::WriteAllText((Join-Path $Context.RunDirectory 'result.json'), ($summary | ConvertTo-Json -Depth 6), $script:Utf8NoBom)
    $markdown = New-Object System.Collections.Generic.List[string]
    $markdown.Add("# $($Suite.name) - $($Context.RunId)")
    $markdown.Add('')
    $markdown.Add("Profile: $($Context.Config.profile). Passed $($summary.passed), failed $($summary.failed), not run $($summary.notRun). Shutdown: $Shutdown.")
    if ($FatalError) { $markdown.Add(''); $markdown.Add("**Run error:** $FatalError") }
    if (@($Dumps).Count -gt 0) { $markdown.Add(''); $markdown.Add("**Crash dumps:** $($Dumps -join ', ')") }
    $markdown.Add('')
    $markdown.Add('| Test | Status | Iterations | Seconds | Detail |')
    $markdown.Add('|---|---|---:|---:|---|')
    foreach ($result in $Results) {
        $markdown.Add("| $($result.id) | $($result.status) | $($result.iterations) | $($result.seconds) | $($result.message -replace '\|', '/') |")
    }
    [System.IO.File]::WriteAllLines((Join-Path $Context.RunDirectory 'report.md'), $markdown.ToArray(), $script:Utf8NoBom)
    return $summary
}

Export-ModuleMember -Function *
