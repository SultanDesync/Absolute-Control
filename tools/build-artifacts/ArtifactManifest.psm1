Set-StrictMode -Version Latest

$script:SchemaVersion = 1
$script:ProductName = 'Absolute Control Panel'
$script:MovieDestination = 'Interface/AbsoluteControlPanelMenu.swf'

function Get-ArtifactRoleDefinition {
    param([Parameter(Mandatory = $true)][string]$ArtifactRole)

    switch ($ArtifactRole) {
        'release' {
            return [pscustomobject]@{
                Role = 'release'
                Basename = 'AbsoluteControlPanel'
                Target = 'AbsoluteControlPanel'
                Destination = 'SFSE/Plugins/AbsoluteControlPanel.dll'
                Packageable = $true
            }
        }
        'research-dev' {
            return [pscustomobject]@{
                Role = 'research-dev'
                Basename = 'AbsoluteControlPanelResearchDev'
                Target = 'AbsoluteControlPanelResearchDev'
                Destination = 'SFSE/Plugins/AbsoluteControlPanelResearchDev.dll'
                Packageable = $false
            }
        }
        default { throw "Unknown artifact role '$ArtifactRole'. Expected release or research-dev." }
    }
}

function Get-NormalizedRelativePath {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string]$Path
    )

    $root = [IO.Path]::GetFullPath($RepositoryRoot).TrimEnd(
        [IO.Path]::DirectorySeparatorChar,
        [IO.Path]::AltDirectorySeparatorChar)
    $full = [IO.Path]::GetFullPath($Path)
    $prefix = $root + [IO.Path]::DirectorySeparatorChar
    if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Artifact path is outside the repository: $full"
    }
    return $full.Substring($prefix.Length).Replace('\', '/')
}

function Resolve-ManifestArtifactPath {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if ([IO.Path]::IsPathRooted($RelativePath) -or
        $RelativePath -match '(^|[\\/])\.\.([\\/]|$)') {
        throw "$Label path must be workspace-relative and cannot traverse parent directories: $RelativePath"
    }
    $root = [IO.Path]::GetFullPath($RepositoryRoot).TrimEnd(
        [IO.Path]::DirectorySeparatorChar,
        [IO.Path]::AltDirectorySeparatorChar)
    $full = [IO.Path]::GetFullPath((Join-Path $root $RelativePath))
    $prefix = $root + [IO.Path]::DirectorySeparatorChar
    if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label path resolves outside the repository: $RelativePath"
    }
    return $full
}

function Get-RequiredProperty {
    param(
        [Parameter(Mandatory = $true)]$Object,
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Context
    )

    if ($null -eq $Object -or
        -not ($Object.PSObject.Properties.Name -contains $Name) -or
        $null -eq $Object.$Name -or
        ($Object.$Name -is [string] -and [string]::IsNullOrWhiteSpace($Object.$Name))) {
        throw "Artifact manifest is missing $Context.$Name. Regenerate it after a successful build."
    }
    return $Object.$Name
}

function Assert-ArtifactPluginDirectory {
    param(
        [Parameter(Mandatory = $true)][string]$PluginPath,
        [Parameter(Mandatory = $true)]$Definition
    )

    $directory = Split-Path -Parent $PluginPath
    $legacy = @(Get-ChildItem -LiteralPath $directory -File -Filter 'AbsoluteControlPanelResearch.dll')
    if ($legacy.Count -gt 0) {
        throw "Legacy product DLL is present beside the canonical output: $($legacy[0].FullName). Run 'xmake clean -a' (or remove that exact ignored stale output) and rebuild; deployment will never select it."
    }
    $expectedName = "$($Definition.Basename).dll"
    $expected = @(Get-ChildItem -LiteralPath $directory -File -Filter $expectedName)
    if ($expected.Count -ne 1 -or $expected[0].Name -cne $expectedName) {
        throw "Expected exactly one $($Definition.Role) DLL named $expectedName in $directory; found $($expected.Count)."
    }
}

function Test-ArtifactHash {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$ExpectedHash,
        [Parameter(Mandatory = $true)][long]$ExpectedSize,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label recorded by the artifact manifest is missing: $Path"
    }
    $item = Get-Item -LiteralPath $Path
    if ($item.Length -ne $ExpectedSize) {
        throw "$Label size changed after the manifest was created (expected $ExpectedSize, observed $($item.Length)): $Path"
    }
    $observed = (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash
    if ($observed -ine $ExpectedHash) {
        throw "$Label hash changed after the manifest was created (expected $ExpectedHash, observed $observed): $Path"
    }
}

function Assert-CanonicalSourceRecords {
    param(
        [Parameter(Mandatory = $true)]$Records,
        [Parameter(Mandatory = $true)]$Expected,
        [Parameter(Mandatory = $true)][string]$Context
    )

    $actualRecords = @($Records)
    $expectedRecords = @($Expected)
    if ($actualRecords.Count -ne $expectedRecords.Count) {
        throw "$Context sources[] is incomplete; expected $($expectedRecords.Count) entries, observed $($actualRecords.Count)."
    }
    $previous = $null
    for ($index = 0; $index -lt $actualRecords.Count; ++$index) {
        $record = $actualRecords[$index]
        $path = [string](Get-RequiredProperty $record 'path' "$Context.sources[$index]")
        $hash = [string](Get-RequiredProperty $record 'sha256' "$Context.sources[$index]")
        if ([IO.Path]::IsPathRooted($path) -or
            $path -match '(^|[\/])\.\.([\/]|$)') {
            throw "$Context sources[] paths must be source-root-relative: $path"
        }
        if ($path.Contains('\')) {
            throw "$Context sources[] paths must use canonical forward slashes: $path"
        }
        if ($hash -notmatch '^[A-Fa-f0-9]{64}$') {
            throw "$Context sources[$index].sha256 is not a SHA-256 digest."
        }
        if ($null -ne $previous -and
            [string]::CompareOrdinal([string]$previous, $path) -ge 0) {
            throw "$Context sources[] is not in canonical ordinal path order."
        }
        $expectedPath = [string]$expectedRecords[$index].path
        $expectedHash = [string]$expectedRecords[$index].sha256
        if ($path -cne $expectedPath) {
            throw "$Context sources[] is incomplete or reordered at index $index; expected '$expectedPath', observed '$path'."
        }
        if ($hash -ine $expectedHash) {
            throw "$Context source hash changed for '$path' (expected $expectedHash, observed $hash)."
        }
        $previous = $path
    }
}

function Test-AcpInterfaceProvenance {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [switch]$PassThru
    )

    $root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
    $sourceRoot = Join-Path $root 'interface\src'
    $rootSourcePath = Join-Path $sourceRoot 'AbsoluteControlPanelMenu.as'
    $moviePath = Join-Path $root 'interface\dist\AbsoluteControlPanelMenu.swf'
    $metadataPath = Join-Path $root 'interface\dist\AbsoluteControlPanelMenu.build.json'
    $sourceProvenanceModule = Join-Path $root 'interface\build\SourceProvenance.psm1'
    foreach ($required in @(
            $sourceRoot, $rootSourcePath, $moviePath, $metadataPath,
            $sourceProvenanceModule)) {
        if (-not (Test-Path -LiteralPath $required)) {
            throw "Scaleform source/dist provenance input is missing: $required"
        }
    }

    Import-Module -Name $sourceProvenanceModule -Force
    $observedSources = Get-ActionScriptSourceProvenance -SourceRoot $sourceRoot
    try {
        $metadata = Get-Content -Raw -LiteralPath $metadataPath | ConvertFrom-Json
    } catch {
        throw "Scaleform build metadata is not valid JSON: $metadataPath. $($_.Exception.Message)"
    }
    $metadataTreeHash = [string](Get-RequiredProperty `
        $metadata 'sourceTreeSha256' 'interface build metadata')
    if ($metadataTreeHash -notmatch '^[A-Fa-f0-9]{64}$' -or
        $metadataTreeHash -ine $observedSources.sourceTreeSha256) {
        throw "Scaleform source-tree hash is stale (metadata $metadataTreeHash, observed $($observedSources.sourceTreeSha256)). Rebuild the interface."
    }
    $metadataRecords = Get-RequiredProperty `
        $metadata 'sources' 'interface build metadata'
    Assert-CanonicalSourceRecords `
        -Records $metadataRecords `
        -Expected $observedSources.sources `
        -Context 'Scaleform build metadata'

    $movieHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $moviePath).Hash
    $metadataOutputHash = [string](Get-RequiredProperty `
        $metadata 'outputSha256' 'interface build metadata')
    if ($metadataOutputHash -ine $movieHash) {
        throw "Scaleform dist hash is stale (metadata $metadataOutputHash, observed $movieHash). Rebuild the interface."
    }
    $rootSourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $rootSourcePath).Hash
    if ($metadata.PSObject.Properties.Name -contains 'sourceSha256' -and
        -not [string]::IsNullOrWhiteSpace([string]$metadata.sourceSha256) -and
        [string]$metadata.sourceSha256 -ine $rootSourceHash) {
        throw 'Deprecated Scaleform root sourceSha256 metadata is present but stale. Rebuild the interface or omit that compatibility field.'
    }

    $result = [pscustomobject]@{
        Metadata = $metadata
        MetadataPath = $metadataPath
        MoviePath = $moviePath
        MovieSha256 = $movieHash
        RootSourcePath = $rootSourcePath
        RootSourceSha256 = $rootSourceHash
        SourceRoot = $sourceRoot
        Sources = @($observedSources.sources)
        SourceTreeSha256 = [string]$observedSources.sourceTreeSha256
    }
    if ($PassThru) { return $result }
    Write-Host "Validated complete Scaleform source tree ($($result.Sources.Count) sources): $($result.SourceTreeSha256)"
}

function New-AcpBuildArtifactManifest {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string]$Configuration,
        [Parameter(Mandatory = $true)][string]$ManifestPath,
        [string]$Platform = 'windows',
        [string]$Architecture = 'x64',
        [string]$Version,
        [string[]]$RuntimeVersions,
        [ValidateSet('release', 'research-dev')][string]$ArtifactRole = 'release'
    )

    $root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
    $definition = Get-ArtifactRoleDefinition $ArtifactRole
    $pluginPath = Join-Path $root "build\$Platform\$Architecture\$Configuration\$($definition.Basename).dll"
    $moviePath = Join-Path $root 'interface\dist\AbsoluteControlPanelMenu.swf'
    $movieSourcePath = Join-Path $root 'interface\src\AbsoluteControlPanelMenu.as'
    $movieMetadataPath = Join-Path $root 'interface\dist\AbsoluteControlPanelMenu.build.json'

    foreach ($required in @($pluginPath, $moviePath, $movieSourcePath, $movieMetadataPath)) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
            throw "Cannot create artifact manifest because a required build output or provenance file is missing: $required"
        }
    }
    Assert-ArtifactPluginDirectory -PluginPath $pluginPath -Definition $definition

    $validatedInterface = Test-AcpInterfaceProvenance `
        -RepositoryRoot $root -PassThru
    $movieMetadata = $validatedInterface.Metadata
    $movieSourceHash = $validatedInterface.RootSourceSha256
    $movieHash = $validatedInterface.MovieSha256

    if ([string]::IsNullOrWhiteSpace($Version)) {
        $xmakeText = Get-Content -Raw -LiteralPath (Join-Path $root 'xmake.lua')
        if ($xmakeText -match 'local\s+product_version\s*=\s*"([^"]+)"') {
            $Version = $Matches[1]
        } elseif ($xmakeText -match 'set_version\("([^"]+)"\)') {
            $Version = $Matches[1]
        } else {
            throw 'Product version was not provided and xmake.lua has no canonical product_version or literal set_version(...).'
        }
    }
    if ($null -eq $RuntimeVersions -or $RuntimeVersions.Count -eq 0) {
        $catalogPath = Join-Path $root 'catalog\catalog.json'
        if (Test-Path -LiteralPath $catalogPath -PathType Leaf) {
            $catalog = Get-Content -Raw -LiteralPath $catalogPath | ConvertFrom-Json
            $RuntimeVersions = @($catalog.runtime.verifiedVersions | ForEach-Object { [string]$_ })
        }
    }
    if ($null -eq $RuntimeVersions) { $RuntimeVersions = @() }

    $revision = 'unavailable'
    $dirty = $null
    try {
        $observedRevision = (& git -C $root rev-parse HEAD 2>$null)
        if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($observedRevision)) {
            $revision = $observedRevision.Trim()
            $status = (& git -C $root status --porcelain --untracked-files=no 2>$null)
            $dirty = -not [string]::IsNullOrWhiteSpace(($status -join "`n"))
        }
    } catch {
        # Git identity is useful provenance but not required for source archives.
    }

    $pluginItem = Get-Item -LiteralPath $pluginPath
    $movieItem = Get-Item -LiteralPath $moviePath
    $sourceItem = Get-Item -LiteralPath $movieSourcePath
    $manifest = [ordered]@{
        schemaVersion = $script:SchemaVersion
        product = [ordered]@{
            name = $script:ProductName
            role = $definition.Role
            basename = $definition.Basename
            target = $definition.Target
            packageable = $definition.Packageable
            version = $Version
        }
        build = [ordered]@{
            configuration = $Configuration
            platform = $Platform
            architecture = $Architecture
            generatedAtUtc = [DateTimeOffset]::UtcNow.ToString('o')
            identity = [ordered]@{
                revision = $revision
                dirty = $dirty
            }
        }
        runtime = [ordered]@{
            game = 'Starfield'
            verifiedVersions = @($RuntimeVersions)
        }
        artifacts = [ordered]@{
            plugin = [ordered]@{
                kind = 'sfse-plugin'
                path = Get-NormalizedRelativePath -RepositoryRoot $root -Path $pluginPath
                destination = $definition.Destination
                sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $pluginPath).Hash
                size = $pluginItem.Length
                lastWriteUtc = $pluginItem.LastWriteTimeUtc.ToString('o')
            }
            interface = [ordered]@{
                kind = 'scaleform-movie'
                path = Get-NormalizedRelativePath -RepositoryRoot $root -Path $moviePath
                destination = $script:MovieDestination
                sha256 = $movieHash
                size = $movieItem.Length
                lastWriteUtc = $movieItem.LastWriteTimeUtc.ToString('o')
                source = [ordered]@{
                    path = Get-NormalizedRelativePath -RepositoryRoot $root -Path $movieSourcePath
                    sha256 = $movieSourceHash
                    size = $sourceItem.Length
                }
                provenance = [ordered]@{
                    path = Get-NormalizedRelativePath -RepositoryRoot $root -Path $movieMetadataPath
                    compiler = [string]$movieMetadata.compiler
                    targetPlayer = [string]$movieMetadata.targetPlayer
                    swfVersion = [int]$movieMetadata.swfVersion
                    playerGlobalCommit = [string]$movieMetadata.playerGlobalCommit
                    sourceTreeSha256 = $validatedInterface.SourceTreeSha256
                    sources = @($validatedInterface.Sources | ForEach-Object {
                            [ordered]@{
                                path = [string]$_.path
                                sha256 = [string]$_.sha256
                            }
                        })
                }
            }
        }
    }

    $manifestDirectory = Split-Path -Parent ([IO.Path]::GetFullPath($ManifestPath))
    New-Item -ItemType Directory -Force -Path $manifestDirectory | Out-Null
    [IO.File]::WriteAllText(
        [IO.Path]::GetFullPath($ManifestPath),
        (($manifest | ConvertTo-Json -Depth 12) + [Environment]::NewLine),
        [Text.UTF8Encoding]::new($false))
    return Test-AcpBuildArtifactManifest -RepositoryRoot $root -ManifestPath $ManifestPath -PassThru
}

function Test-AcpBuildArtifactManifest {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string]$ManifestPath,
        [string]$ExpectedConfiguration,
        [ValidateSet('release', 'research-dev')][string]$ExpectedArtifactRole,
        [switch]$PassThru
    )

    $root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
    if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
        throw "Canonical build artifact manifest is missing: $ManifestPath. Build the product and run tools\build-artifacts\New-BuildArtifactManifest.ps1."
    }
    try {
        $manifest = Get-Content -Raw -LiteralPath $ManifestPath | ConvertFrom-Json
    } catch {
        throw "Canonical build artifact manifest is not valid JSON: $ManifestPath. $($_.Exception.Message)"
    }

    $schemaVersion = Get-RequiredProperty $manifest 'schemaVersion' 'root'
    if ([int]$schemaVersion -ne $script:SchemaVersion) {
        throw "Unsupported artifact manifest schemaVersion $schemaVersion; expected $($script:SchemaVersion)."
    }
    $product = Get-RequiredProperty $manifest 'product' 'root'
    $build = Get-RequiredProperty $manifest 'build' 'root'
    $runtime = Get-RequiredProperty $manifest 'runtime' 'root'
    $artifacts = Get-RequiredProperty $manifest 'artifacts' 'root'
    $artifactRole = [string](Get-RequiredProperty $product 'role' 'product')
    $definition = Get-ArtifactRoleDefinition $artifactRole
    if (-not [string]::IsNullOrWhiteSpace($ExpectedArtifactRole) -and
        $artifactRole -cne $ExpectedArtifactRole) {
        throw "Artifact manifest role is '$artifactRole'; expected '$ExpectedArtifactRole'."
    }
    if ((Get-RequiredProperty $product 'name' 'product') -cne $script:ProductName -or
        (Get-RequiredProperty $product 'basename' 'product') -cne $definition.Basename -or
        (Get-RequiredProperty $product 'target' 'product') -cne $definition.Target -or
        [bool](Get-RequiredProperty $product 'packageable' 'product') -ne $definition.Packageable) {
        throw 'Artifact manifest product identity does not match its release/research role.'
    }
    [void](Get-RequiredProperty $product 'version' 'product')
    $configuration = [string](Get-RequiredProperty $build 'configuration' 'build')
    if ($configuration -notin @('debug', 'release', 'releasedbg')) {
        throw "Artifact manifest has unsupported build configuration '$configuration'."
    }
    $platform = [string](Get-RequiredProperty $build 'platform' 'build')
    $architecture = [string](Get-RequiredProperty $build 'architecture' 'build')
    if ($platform -cne 'windows' -or $architecture -cne 'x64') {
        throw "Artifact manifest platform/architecture must be windows/x64; observed $platform/$architecture."
    }
    $generatedAt = [string](Get-RequiredProperty $build 'generatedAtUtc' 'build')
    $parsedTimestamp = [DateTimeOffset]::MinValue
    if (-not [DateTimeOffset]::TryParse($generatedAt, [ref]$parsedTimestamp)) {
        throw "Artifact manifest build.generatedAtUtc is not a timestamp: $generatedAt"
    }
    $identity = Get-RequiredProperty $build 'identity' 'build'
    [void](Get-RequiredProperty $identity 'revision' 'build.identity')
    if (-not ($identity.PSObject.Properties.Name -contains 'dirty')) {
        throw 'Artifact manifest is missing build.identity.dirty.'
    }
    if ((Get-RequiredProperty $runtime 'game' 'runtime') -cne 'Starfield') {
        throw 'Artifact manifest runtime.game must be Starfield.'
    }
    [void](Get-RequiredProperty $runtime 'verifiedVersions' 'runtime')
    if (-not [string]::IsNullOrWhiteSpace($ExpectedConfiguration) -and
        $configuration -cne $ExpectedConfiguration) {
        throw "Artifact manifest configuration is '$configuration'; expected '$ExpectedConfiguration'. Rebuild/regenerate the intended configuration."
    }

    $plugin = Get-RequiredProperty $artifacts 'plugin' 'artifacts'
    $movie = Get-RequiredProperty $artifacts 'interface' 'artifacts'
    if ((Get-RequiredProperty $plugin 'kind' 'artifacts.plugin') -cne 'sfse-plugin' -or
        (Get-RequiredProperty $movie 'kind' 'artifacts.interface') -cne 'scaleform-movie') {
        throw 'Artifact manifest contains unexpected artifact kinds.'
    }
    if ((Get-RequiredProperty $plugin 'destination' 'artifacts.plugin') -cne $definition.Destination) {
        throw "Plugin destination must be $($definition.Destination) for role '$artifactRole'; legacy or alternate filenames are rejected."
    }
    if ((Get-RequiredProperty $movie 'destination' 'artifacts.interface') -cne $script:MovieDestination) {
        throw "Interface destination must be $($script:MovieDestination)."
    }
    $pluginRelative = [string](Get-RequiredProperty $plugin 'path' 'artifacts.plugin')
    $movieRelative = [string](Get-RequiredProperty $movie 'path' 'artifacts.interface')
    if ($pluginRelative -match 'AbsoluteControlPanelResearch\.dll') {
        throw 'Artifact manifest refers to the retired AbsoluteControlPanelResearch product DLL.'
    }
    $pluginPath = Resolve-ManifestArtifactPath $root $pluginRelative 'Plugin'
    $moviePath = Resolve-ManifestArtifactPath $root $movieRelative 'Interface'
    $expectedPluginRelative = "build/$platform/$architecture/$configuration/$($definition.Basename).dll"
    if ($pluginRelative.Replace('\', '/') -cne $expectedPluginRelative) {
        throw "Artifact manifest plugin path must match its build identity: $expectedPluginRelative"
    }
    if ($movieRelative.Replace('\', '/') -cne 'interface/dist/AbsoluteControlPanelMenu.swf') {
        throw 'Artifact manifest interface path must be interface/dist/AbsoluteControlPanelMenu.swf.'
    }
    if ((Split-Path -Leaf $pluginPath) -cne "$($definition.Basename).dll") {
        throw "Artifact manifest plugin path is not the expected $artifactRole DLL: $pluginRelative"
    }
    Assert-ArtifactPluginDirectory -PluginPath $pluginPath -Definition $definition
    Test-ArtifactHash $pluginPath ([string](Get-RequiredProperty $plugin 'sha256' 'artifacts.plugin')) `
        ([long](Get-RequiredProperty $plugin 'size' 'artifacts.plugin')) 'Plugin'
    Test-ArtifactHash $moviePath ([string](Get-RequiredProperty $movie 'sha256' 'artifacts.interface')) `
        ([long](Get-RequiredProperty $movie 'size' 'artifacts.interface')) 'Interface'

    $source = Get-RequiredProperty $movie 'source' 'artifacts.interface'
    $sourceRelative = [string](Get-RequiredProperty $source 'path' 'artifacts.interface.source')
    if ($sourceRelative.Replace('\', '/') -cne 'interface/src/AbsoluteControlPanelMenu.as') {
        throw 'Artifact manifest interface source path is not canonical.'
    }
    $sourcePath = Resolve-ManifestArtifactPath $root $sourceRelative 'Interface source'
    Test-ArtifactHash $sourcePath `
        ([string](Get-RequiredProperty $source 'sha256' 'artifacts.interface.source')) `
        ([long](Get-RequiredProperty $source 'size' 'artifacts.interface.source')) 'Interface source'
    $provenance = Get-RequiredProperty $movie 'provenance' 'artifacts.interface'
    $provenanceRelative = [string](Get-RequiredProperty $provenance 'path' 'artifacts.interface.provenance')
    if ($provenanceRelative.Replace('\', '/') -cne 'interface/dist/AbsoluteControlPanelMenu.build.json') {
        throw 'Artifact manifest interface provenance path is not canonical.'
    }
    foreach ($field in @(
            'compiler', 'targetPlayer', 'swfVersion', 'playerGlobalCommit',
            'sourceTreeSha256', 'sources')) {
        [void](Get-RequiredProperty $provenance $field 'artifacts.interface.provenance')
    }
    $provenancePath = Resolve-ManifestArtifactPath $root $provenanceRelative 'Interface provenance'
    if (-not (Test-Path -LiteralPath $provenancePath -PathType Leaf)) {
        throw "Interface provenance recorded by the artifact manifest is missing: $provenancePath"
    }
    $validatedInterface = Test-AcpInterfaceProvenance `
        -RepositoryRoot $root -PassThru
    if ($validatedInterface.MetadataPath -cne $provenancePath -or
        $validatedInterface.RootSourceSha256 -ine $source.sha256 -or
        $validatedInterface.MovieSha256 -ine $movie.sha256) {
        throw 'Interface build metadata no longer agrees with the source and SWF hashes in the artifact manifest.'
    }
    $manifestTreeHash = [string](Get-RequiredProperty `
        $provenance 'sourceTreeSha256' 'artifacts.interface.provenance')
    if ($manifestTreeHash -ine $validatedInterface.SourceTreeSha256) {
        throw 'Artifact manifest interface sourceTreeSha256 no longer matches the complete ActionScript source tree.'
    }
    Assert-CanonicalSourceRecords `
        -Records (Get-RequiredProperty `
            $provenance 'sources' 'artifacts.interface.provenance') `
        -Expected $validatedInterface.Sources `
        -Context 'Artifact manifest interface provenance'

    $result = [pscustomobject]@{
        Manifest = $manifest
        ArtifactRole = $artifactRole
        PluginPath = $pluginPath
        MoviePath = $moviePath
        MovieSourcePath = $sourcePath
        MovieProvenancePath = $provenancePath
    }
    if ($PassThru) { return $result }
    Write-Host "Validated $artifactRole host artifacts for $($product.version) ($configuration)."
}

Export-ModuleMember -Function New-AcpBuildArtifactManifest, `
    Test-AcpBuildArtifactManifest, Test-AcpInterfaceProvenance
