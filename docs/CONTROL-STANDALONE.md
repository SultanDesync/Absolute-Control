# Absolute Control standalone menu host

Absolute Control can be distributed separately from the all-in-one Absolute Suite. The standalone
archive contains only the shared native menu host used by Absolute Power, AbsoluteHOTAS, Absolute
Head Tracking, AbsoluteZero, and third-party providers built against the SDK.

## Requirement listing

- **Name:** Absolute Control — Standalone Menu Host
- **Version:** 0.3.0-beta.1
- **Archive:** `Absolute-Control-v0.3.0-beta.1-Standalone.zip`
- **Summary:** Shared Pause Menu configuration host for modular Absolute providers. Contains no
  gameplay module.
- **SHA-256:** `5CD253DF9FB1F57E9197DBD889D23CAABECAFA5A2B7A366CF976A9134AC010FC`

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

This standalone host is identical to the `00 Core` component in Absolute Suite
0.3.0-beta.1. It contains the additive radial-response and head-pose surfaces,
plus visible clearing for provider bindings.
