# Absolute Control standalone menu host

Absolute Control can be distributed separately from the all-in-one Absolute Suite. The standalone
archive contains only the shared native menu host used by Absolute Power, AbsoluteHOTAS, Absolute
Head Tracking, AbsoluteZero, and third-party providers built against the SDK.

## Requirement listing

- **Name:** Absolute Control — Standalone Menu Host
- **Version:** 0.2.0-beta.1
- **Archive:** `Absolute-Control-v0.2.0-beta.1-Standalone.zip`
- **Summary:** Shared Pause Menu configuration host for modular Absolute providers. Contains no
  gameplay module.
- **SHA-256:** `3FC27A210AC5ABD7DD41020A0B5861924F4DEA5FECAC7CDCF1FBA23E0416F4A0`

For the Absolute Power Nexus requirement entry, use:

> Required for the advertised in-game MOD OPTIONS editor. Absolute Power's gameplay backend also
> retains its INI and shortcut fallback when the menu host is absent.

## Requirements

- Starfield 1.16.244
- SFSE 0.2.20 or later
- Address Library for SFSE Plugins

## Archive contents

- `SFSE/Plugins/AbsoluteControlPanel.dll`
- `SFSE/Plugins/AbsoluteControlPanel.ini`
- `Interface/AbsoluteControlPanelMenu.swf`

The package is directly installable with MO2 or Vortex. It does not contain Absolute Power,
AbsoluteHOTAS, Absolute Head Tracking, AbsoluteZero, user-owned custom configuration, research
plugins, SDK files, or debugging artifacts.

Do not enable this package alongside the Absolute Control core from the all-in-one installer. They
are the same host and would cause one mod-manager entry to overwrite the other.

## Provenance

The payload is byte-for-byte identical to the required `00 Core` component of Absolute Suite
0.2.0-beta.1, which was assembled from validated Absolute Control commit
`9c2f42929563372c433bc222710fced64721036c`.

