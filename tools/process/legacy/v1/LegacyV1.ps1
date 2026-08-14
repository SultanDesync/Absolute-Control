function Assert-LegacyV1OptIn {
    param(
        [Parameter(Mandatory = $true)][bool]$Allowed,
        [Parameter(Mandatory = $true)][string]$EntryPoint
    )

    if (-not $Allowed) {
        throw "$EntryPoint belongs to archived disposable-builder-v1. Use tools/process/validate-current.cmd for product validation, or supply -AllowLegacyV1 only to reproduce historical builder evidence."
    }
    Write-Warning "$EntryPoint is running archived disposable-builder-v1. Its results are historical and are not current product, runtime, UX, or release evidence."
}
