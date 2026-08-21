# Absolute Control SDK beta access

The integration SDK is currently distributed as a coordinated private beta. Mod authors who want
to place an existing or new SFSE plugin inside the shared **MOD OPTIONS** menu should message the
Absolute Control author through Nexus Mods to apply. The public SDK release is coming soon.

## What to include in an application

- mod name and Nexus or project link;
- current released version and supported Starfield/SFSE versions;
- existing configuration surface, persistence format, and fallback UI;
- pages, settings, bindings, telemetry, or shared services proposed for integration;
- whether the plugin is open source and which build system it uses;
- intended beta deployment window and a contact route for compatibility follow-up.

Do not send user configuration files, logs containing account or local-path information, device
serials, access tokens, or private signing material.

## Coordinated deployment

Each accepted integration receives a specific SDK package version and host baseline. The author
records the module ID, consumed API generations, fallback behavior, tested host version, and
deployment state in `integration-registry.json`. Contact information remains outside the repository.

Beta consumers must:

- keep gameplay and configuration ownership in their own plugin;
- continue operating when Absolute Control is absent, old, incompatible, or unavailable;
- query every API dynamically and size/capability-check optional tails;
- retain a provider-owned fallback configuration route during the beta;
- report the exact SDK version and host version with integration issues; and
- coordinate a compatibility check before publishing an update that changes its registered menu.

The SDK package version and ABI version are separate. Beta package revisions may advance while
the stable configuration ABI remains version 1. Experimental live and composition APIs can change
before their public freeze.
