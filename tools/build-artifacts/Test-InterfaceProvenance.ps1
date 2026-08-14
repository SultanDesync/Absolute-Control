[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Import-Module (Join-Path $PSScriptRoot 'ArtifactManifest.psm1') -Force
Test-AcpInterfaceProvenance -RepositoryRoot $repositoryRoot
