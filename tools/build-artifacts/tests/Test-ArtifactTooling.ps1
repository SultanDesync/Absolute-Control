[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$modulePath = (Resolve-Path (Join-Path $PSScriptRoot '..\ArtifactManifest.psm1')).Path
Import-Module $modulePath -Force
$sourceProvenanceModule = (Resolve-Path `
    (Join-Path $PSScriptRoot '..\..\..\interface\build\SourceProvenance.psm1')).Path
Import-Module $sourceProvenanceModule -Force
$fixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ("acp-artifact-test-" + [Guid]::NewGuid().ToString('N'))

function Write-Utf8File {
    param([string]$Path, [string]$Contents)
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
    [IO.File]::WriteAllText($Path, $Contents, [Text.UTF8Encoding]::new($false))
}

function Assert-Throws {
    param([scriptblock]$Action, [string]$Pattern)
    try {
        & $Action
    } catch {
        if ($_.Exception.Message -notmatch $Pattern) {
            throw "Expected error matching '$Pattern', observed: $($_.Exception.Message)"
        }
        return
    }
    throw "Expected action to fail with '$Pattern'."
}

try {
    $plugin = Join-Path $fixtureRoot 'build\windows\x64\release\AbsoluteControlPanel.dll'
    $source = Join-Path $fixtureRoot 'interface\src\AbsoluteControlPanelMenu.as'
    $helperSource = Join-Path $fixtureRoot 'interface\src\acp\ui\FixtureHelper.as'
    $movie = Join-Path $fixtureRoot 'interface\dist\AbsoluteControlPanelMenu.swf'
    $metadata = Join-Path $fixtureRoot 'interface\dist\AbsoluteControlPanelMenu.build.json'
    $fixtureProvenanceModule = Join-Path $fixtureRoot `
        'interface\build\SourceProvenance.psm1'
    $catalog = Join-Path $fixtureRoot 'catalog\catalog.json'
    $manifest = Join-Path $fixtureRoot 'build\artifact-manifests\AbsoluteControlPanel.artifacts.json'
    Write-Utf8File $plugin 'canonical-plugin-fixture'
    New-Item -ItemType Directory -Force `
        -Path (Split-Path -Parent $fixtureProvenanceModule) | Out-Null
    Copy-Item -LiteralPath $sourceProvenanceModule `
        -Destination $fixtureProvenanceModule
    Write-Utf8File $source 'package fixture { public class AbsoluteControlPanelMenu {} }'
    $helperContents = 'package acp.ui { public class FixtureHelper {} }'
    Write-Utf8File $helperSource $helperContents
    Write-Utf8File $movie 'canonical-swf-fixture'
    $sourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash
    $movieHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $movie).Hash
    $sourceProvenance = Get-ActionScriptSourceProvenance `
        -SourceRoot (Join-Path $fixtureRoot 'interface\src')
    $canonicalMetadata = [ordered]@{
        compiler = 'fixture compiler'
        targetPlayer = '11.5'
        swfVersion = 18
        playerGlobalCommit = 'fixture'
        # Deprecated root-only compatibility metadata is deliberately retained;
        # sources[] and sourceTreeSha256 below are authoritative.
        sourceSha256 = $sourceHash
        sourceTreeSha256 = $sourceProvenance.sourceTreeSha256
        sources = $sourceProvenance.sources
        outputSha256 = $movieHash
    }
    $canonicalMetadataText = ($canonicalMetadata | ConvertTo-Json -Depth 6) + "`n"
    Write-Utf8File $metadata $canonicalMetadataText
    Write-Utf8File $catalog '{"runtime":{"verifiedVersions":["1.2.3.4"]}}'
    Write-Utf8File (Join-Path $fixtureRoot 'xmake.lua') @'
local product_version = "9.8.8-dev"
set_version(product_version)
'@

    $result = New-AcpBuildArtifactManifest `
        -RepositoryRoot $fixtureRoot `
        -Configuration release `
        -ManifestPath $manifest
    if ($result.Manifest.product.basename -cne 'AbsoluteControlPanel' -or
        $result.Manifest.product.target -cne 'AbsoluteControlPanel' -or
        $result.Manifest.product.version -cne '9.8.8-dev') {
        throw 'Generated manifest does not contain the canonical product identity.'
    }
    if ([IO.Path]::IsPathRooted([string]$result.Manifest.artifacts.plugin.path)) {
        throw 'Generated manifest leaked an absolute workstation path.'
    }
    [void](Test-AcpBuildArtifactManifest `
        -RepositoryRoot $fixtureRoot `
        -ManifestPath $manifest `
        -ExpectedConfiguration release `
        -PassThru)

    Write-Utf8File $helperSource 'package acp.ui { public class FixtureHelper { public var changed:Boolean; } }'
    Assert-Throws {
        Test-AcpBuildArtifactManifest `
            -RepositoryRoot $fixtureRoot -ManifestPath $manifest
    } 'source hash changed|source-tree hash is stale'
    Write-Utf8File $helperSource $helperContents

    $omitted = $canonicalMetadataText | ConvertFrom-Json
    $omitted.sources = @($omitted.sources | Select-Object -First 1)
    Write-Utf8File $metadata (($omitted | ConvertTo-Json -Depth 6) + "`n")
    Assert-Throws {
        Test-AcpInterfaceProvenance -RepositoryRoot $fixtureRoot
    } 'sources\[\] is incomplete'

    $reordered = $canonicalMetadataText | ConvertFrom-Json
    $reordered.sources = @($reordered.sources | Sort-Object path -Descending)
    Write-Utf8File $metadata (($reordered | ConvertTo-Json -Depth 6) + "`n")
    Assert-Throws {
        Test-AcpInterfaceProvenance -RepositoryRoot $fixtureRoot
    } 'canonical ordinal path order|reordered'

    $rooted = $canonicalMetadataText | ConvertFrom-Json
    $rooted.sources[0].path = 'C:\private\AbsoluteControlPanelMenu.as'
    Write-Utf8File $metadata (($rooted | ConvertTo-Json -Depth 6) + "`n")
    Assert-Throws {
        Test-AcpInterfaceProvenance -RepositoryRoot $fixtureRoot
    } 'source-root-relative'

    $withoutDeprecatedRootHash = $canonicalMetadataText | ConvertFrom-Json
    $withoutDeprecatedRootHash.PSObject.Properties.Remove('sourceSha256')
    Write-Utf8File $metadata `
        (($withoutDeprecatedRootHash | ConvertTo-Json -Depth 6) + "`n")
    [void](Test-AcpInterfaceProvenance `
        -RepositoryRoot $fixtureRoot -PassThru)
    Write-Utf8File $metadata $canonicalMetadataText

    $stage = Join-Path $fixtureRoot 'stage'
    Write-Utf8File (Join-Path $stage 'README.txt') 'fixture package'
    Write-Utf8File (Join-Path $stage 'SFSE\Plugins\AbsoluteControlPanel.ini') '[ControlPanel]'
    New-Item -ItemType Directory -Force -Path `
        (Join-Path $stage 'SFSE\Plugins'), (Join-Path $stage 'Interface') | Out-Null
    Copy-Item -LiteralPath $plugin -Destination `
        (Join-Path $stage 'SFSE\Plugins\AbsoluteControlPanel.dll')
    Copy-Item -LiteralPath $movie -Destination `
        (Join-Path $stage 'Interface\AbsoluteControlPanelMenu.swf')
    & (Join-Path $PSScriptRoot '..\Test-PackageContent.ps1') `
        -RepositoryRoot $fixtureRoot `
        -ArtifactManifest $manifest `
        -StageRoot $stage

    $archive = Join-Path $fixtureRoot 'package.zip'
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive
    & (Join-Path $PSScriptRoot '..\Test-PackageContent.ps1') `
        -RepositoryRoot $fixtureRoot `
        -ArtifactManifest $manifest `
        -ArchivePath $archive

    Write-Utf8File `
        (Join-Path $stage 'SFSE\Plugins\AbsoluteControlPanelResearch.dll') `
        'retired-package-fixture'
    Assert-Throws {
        & (Join-Path $PSScriptRoot '..\Test-PackageContent.ps1') `
            -RepositoryRoot $fixtureRoot `
            -ArtifactManifest $manifest `
            -StageRoot $stage
    } 'Package contents are not canonical'

    $researchPlugin = Join-Path $fixtureRoot `
        'build\windows\x64\release\AbsoluteControlPanelResearchDev.dll'
    $researchManifest = Join-Path $fixtureRoot `
        'build\artifact-manifests\AbsoluteControlPanelResearchDev.artifacts.json'
    Write-Utf8File $researchPlugin 'research-development-plugin-fixture'
    $research = New-AcpBuildArtifactManifest `
        -RepositoryRoot $fixtureRoot `
        -Configuration release `
        -ManifestPath $researchManifest `
        -ArtifactRole research-dev
    if ($research.Manifest.product.role -cne 'research-dev' -or
        $research.Manifest.product.target -cne 'AbsoluteControlPanelResearchDev' -or
        [bool]$research.Manifest.product.packageable) {
        throw 'Research manifest is not explicitly marked non-release/non-packageable.'
    }
    [void](Test-AcpBuildArtifactManifest `
        -RepositoryRoot $fixtureRoot `
        -ManifestPath $researchManifest `
        -ExpectedArtifactRole research-dev `
        -PassThru)
    Assert-Throws {
        & (Join-Path $PSScriptRoot '..\Test-PackageContent.ps1') `
            -RepositoryRoot $fixtureRoot `
            -ArtifactManifest $researchManifest `
            -StageRoot $stage
    } "expected 'release'"

    Write-Utf8File $plugin 'tampered-plugin-fixture'
    Assert-Throws {
        Test-AcpBuildArtifactManifest -RepositoryRoot $fixtureRoot -ManifestPath $manifest
    } 'changed after the manifest'

    Write-Utf8File $plugin 'canonical-plugin-fixture'
    [void](New-AcpBuildArtifactManifest `
        -RepositoryRoot $fixtureRoot `
        -Configuration release `
        -ManifestPath $manifest)
    Write-Utf8File `
        (Join-Path $fixtureRoot 'build\windows\x64\release\AbsoluteControlPanelResearch.dll') `
        'retired-legacy-fixture'
    Assert-Throws {
        Test-AcpBuildArtifactManifest -RepositoryRoot $fixtureRoot -ManifestPath $manifest
    } 'Legacy product DLL is present'

    Remove-Item -LiteralPath `
        (Join-Path $fixtureRoot 'build\windows\x64\release\AbsoluteControlPanelResearch.dll') `
        -Force
    $json = Get-Content -Raw -LiteralPath $manifest | ConvertFrom-Json
    $json.artifacts.plugin.path = 'C:\private\AbsoluteControlPanel.dll'
    [IO.File]::WriteAllText(
        $manifest,
        (($json | ConvertTo-Json -Depth 12) + "`n"),
        [Text.UTF8Encoding]::new($false))
    Assert-Throws {
        Test-AcpBuildArtifactManifest -RepositoryRoot $fixtureRoot -ManifestPath $manifest
    } 'workspace-relative'

    Write-Host 'Artifact tooling fixture tests passed.'
} finally {
    if (Test-Path -LiteralPath $fixtureRoot) {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force
    }
}
