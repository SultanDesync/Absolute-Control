# Build artifact integrity

`build/artifact-manifests/AbsoluteControlPanel.artifacts.json` is the sole authority for packaging
the release host. `AbsoluteControlPanelResearchDev.artifacts.json` is the corresponding authority
for research-harness deployment and is explicitly marked non-packageable. Both are generated output
and remain ignored with the rest of `build/`.
The manifest records the canonical product identity, target, build configuration, supported
runtime versions, repository revision/dirty state, generation time, workspace-relative artifact
paths, sizes, timestamps, and SHA-256 hashes. For the Scaleform movie, the authoritative source
identity is the canonical, ordered `sources[]` inventory plus `sourceTreeSha256`; this covers the
document class and every helper under `interface/src`. The root-only `sourceSha256` field in the
interface compiler metadata is retained as optional deprecated compatibility metadata and is not
accepted in place of the complete inventory.

The `product_version` value in `xmake.lua` is the product-version authority. Xmake uses it for the
compiled API table, and manifest generation reads the same value for artifact identity.

Generate it after building the canonical `AbsoluteControlPanel` target:

```powershell
.\tools\build-artifacts\New-BuildArtifactManifest.ps1 -Configuration release
.\tools\build-artifacts\New-BuildArtifactManifest.ps1 -Configuration releasedbg -ArtifactRole research-dev
```

Validate an existing manifest and every recorded byte before consuming it:

```powershell
.\tools\build-artifacts\Test-BuildArtifactManifest.ps1 -ExpectedConfiguration release
```

The generator and validator fail closed when the canonical product is missing, when the retired
`AbsoluteControlPanelResearch.dll` is present beside it, when a path is rooted or escapes the
workspace, when Scaleform provenance is stale, or when any size/hash has changed. They never scan
multiple build modes to find a plausible DLL. Source inventories also fail closed on omitted,
duplicated, reordered, rooted, traversing, or helper-only-tampered entries.

`deploy-probe.ps1` consumes only the research-development manifest and refuses to co-load its sole
host with either the canonical or retired host. `package-compatibility-test.ps1` consumes only a
release manifest marked packageable. Package validation permits exactly the canonical DLL, INI,
SWF, and README paths and verifies the manifest-owned DLL/SWF hashes in both staging tree and ZIP.
