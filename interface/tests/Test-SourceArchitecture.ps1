[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$interfaceRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$sourceRoot = Join-Path $interfaceRoot 'src'
$rootPath = Join-Path $sourceRoot 'AbsoluteControlPanelMenu.as'
$metadataPath = Join-Path $interfaceRoot 'dist\AbsoluteControlPanelMenu.build.json'
$provenanceModule = Join-Path $interfaceRoot 'build\SourceProvenance.psm1'

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

$sourceFiles = @(
    Get-ChildItem -LiteralPath $sourceRoot -Recurse -File -Filter '*.as' |
        Sort-Object { $_.FullName.Substring($sourceRoot.Length).Replace('\', '/') }
)
$rootSource = Get-Content -Raw -LiteralPath $rootPath

$expectedPublicMethods = @(
    'AbsoluteControlPanelMenu',
    'applyModel',
    'handlePointerClick',
    'handlePointerDown',
    'handlePointerMove',
    'handlePointerUp',
    'handlePointerWheel',
    'onCodeObjCreate',
    'setInputMode'
) | Sort-Object
$observedPublicMethods = @(
    [regex]::Matches($rootSource, 'public\s+function\s+([A-Za-z0-9_]+)') |
        ForEach-Object { $_.Groups[1].Value } |
        Sort-Object
)
Assert-True (($expectedPublicMethods -join ',') -ceq ($observedPublicMethods -join ',')) `
    "Document-class public surface changed. Expected $($expectedPublicMethods -join ', '); observed $($observedPublicMethods -join ', ')."

foreach ($required in @(
        'BridgeCommandDispatcher',
        'ChoiceInputRouter',
        'ControlWidgets.activate',
        'MenuSelectionState',
        'MenuShellRenderer',
        'ModalInputRouter',
        'PointerInteraction',
        'SliderWriteCoordinator'
    )) {
    Assert-True ($rootSource.Contains($required)) "Document coordinator is missing '$required'."
}

foreach ($forbidden in @(
        'private static const GLYPHS',
        'function addModuleButton',
        'function activeModulePages',
        'function drawControlWidget',
        'function drawFooter',
        'function drawHelp',
        'hitTestPoint(stageX, stageY, false)',
        'draggingControlId'
    )) {
    Assert-True (-not $rootSource.Contains($forbidden)) `
        "Document coordinator reclaimed component responsibility '$forbidden'."
}

$responsibilityOwners = [ordered]@{
    'private static const GLYPHS' = 'acp/ui/PixelTextRenderer.as'
    'function modulePageStarts' = 'acp/ui/MenuSelectionState.as'
    'function addModuleButton' = 'acp/ui/MenuShellRenderer.as'
    'function drawFooter' = 'acp/ui/MenuShellRenderer.as'
    'function drawToggle' = 'acp/ui/ControlWidgets.as'
    'function drawSlider' = 'acp/ui/ControlWidgets.as'
    'hitTestPoint(stageX, stageY, false)' = 'acp/ui/PointerInteraction.as'
    'function wheelRegion' = 'acp/ui/PointerInteraction.as'
    'bridge.dispatch(1, command' = 'acp/ui/BridgeCommandDispatcher.as'
    'Number(write.expectedGeneration) != Number(model.generation)' = `
        'acp/ui/SliderWriteCoordinator.as'
}
foreach ($entry in $responsibilityOwners.GetEnumerator()) {
    $owners = @(
        $sourceFiles |
            Where-Object { (Get-Content -Raw -LiteralPath $_.FullName).Contains($entry.Key) } |
            ForEach-Object {
                $_.FullName.Substring($sourceRoot.Length + 1).Replace('\', '/')
            }
    )
    Assert-True ($owners.Count -eq 1 -and $owners[0] -ceq $entry.Value) `
        "Responsibility '$($entry.Key)' must be owned only by $($entry.Value); observed $($owners -join ', ')."
}

$controlWidgets = Get-Content -Raw -LiteralPath (Join-Path $sourceRoot 'acp\ui\ControlWidgets.as')
$selectionState = Get-Content -Raw -LiteralPath (Join-Path $sourceRoot 'acp\ui\MenuSelectionState.as')
$pointerInteraction = Get-Content -Raw -LiteralPath (Join-Path $sourceRoot 'acp\ui\PointerInteraction.as')
$shellRenderer = Get-Content -Raw -LiteralPath (Join-Path $sourceRoot 'acp\ui\MenuShellRenderer.as')
Assert-True (-not $controlWidgets.Contains('PointerInteraction')) `
    'Control widgets must not depend on the pointer controller; registration crosses a callback.'
Assert-True (-not $selectionState.Contains('ControlWidgets') -and `
    -not $selectionState.Contains('PointerInteraction') -and `
    -not $selectionState.Contains('MenuShellRenderer')) `
    'Selection state must remain independent of input and rendering implementations.'
Assert-True ($pointerInteraction.Contains('ControlWidgets.writeSliderFromPointer')) `
    'Pointer drag handling must reuse the semantic slider conversion rather than duplicate it.'
Assert-True ($shellRenderer.Contains('ControlWidgets.draw') -and `
    $shellRenderer.Contains('VectorTextRenderer') -and `
    -not $shellRenderer.Contains('PixelTextRenderer')) `
    'Shell composition must delegate widget and glyph rendering to their component owners.'

$rootLines = @(Get-Content -LiteralPath $rootPath).Count
Assert-True ($rootLines -lt 500) `
    "Document coordinator has grown to $rootLines lines; move implementation into its owning component."

$metadata = Get-Content -Raw -LiteralPath $metadataPath | ConvertFrom-Json
Import-Module -Name $provenanceModule -Force
$provenance = Get-ActionScriptSourceProvenance -SourceRoot $sourceRoot
Assert-True ($metadata.sourceTreeSha256 -ceq $provenance.sourceTreeSha256) `
    'Scaleform source-tree provenance is stale; rebuild the interface.'
Assert-True ((@($metadata.sources | ForEach-Object { $_.path }) -join ',') -ceq `
        (@($provenance.sources | ForEach-Object { $_.path }) -join ',')) `
    'Scaleform provenance does not enumerate the complete ActionScript source tree.'

Write-Output "Scaleform source architecture verified ($($sourceFiles.Count) sources; root $rootLines lines)."
