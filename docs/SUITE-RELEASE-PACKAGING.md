# Absolute Suite all-in-one packaging

The Absolute Control Nexus page may publish one guided all-in-one without merging the suite's
runtime or configuration ownership. Absolute Control is the required FOMOD core. AbsoluteHOTAS,
Absolute Power, Absolute Head Tracking, and AbsoluteZero Mouse Steering remain independently
selectable and retain their standalone pages, versions, configuration files, and support boundaries.

The presentation and document style follows the established versioned AbsoluteHOTAS release
folders: one release archive beside `README.txt`, `CHANGELOG.txt`, and Nexus-formatted copy. The
all-in-one adds an external and embedded `AbsoluteSuite.manifest.json` because a multi-repository
bundle must identify the exact component commits and bytes it contains.

## Package contract

The component inventory is declared in `packaging/suite/suite-components.json`. The archive may
contain only:

- the required Absolute Control DLL, SWF, and mod-owned default INI;
- selected-module DLLs and mod-owned default INIs beneath their FOMOD source folders;
- `fomod/info.xml` and `fomod/ModuleConfig.xml`; and
- the suite manifest.

It must not contain custom INIs, HOTAS Profiles, logs, PDBs, research builds, source, or retired
Absolute Workbench descriptors. The installer descriptions explicitly warn about defaults that can
change behavior immediately, including Power's 1-4 shortcuts and AbsoluteZero mouse centering.

## Build a candidate

Build every component's release target from a clean committed tree first. Then, from the
Absolute-Control repository:

```powershell
.\tools\release\New-SuiteRelease.ps1 -BundleVersion 0.2.0-beta.1
.\tools\release\Test-SuiteRelease.ps1 -ReleaseDirectory .\releases\v0.2.0-beta.1
```

The packager fails when a repository is dirty, when a DLL/SWF predates its source commit, when an
expected artifact is absent, when output would escape the repository, or when a release directory
already contains files. `-AllowDirty` and `-AllowStaleArtifacts` exist only for local development of
the packaging workflow; do not use them for a Nexus artifact.

The generated ZIP has stable entry ordering and timestamps. Identical component bytes, component
commits, definitions, templates, and bundle version therefore produce the same archive hash.

## Versioning

The suite version is independent of component versions. Updating any component requires a new suite
version and regenerated manifest even if the other component versions do not change. Standalone
files may advance independently; users must not enable duplicate copies of the same DLL from both
the all-in-one and another mod-manager entry.
