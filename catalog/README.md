# Absolute Control Panel catalogue MCP

> **Status:** Current machine-readable capability ledger. Confidence values must agree with
> `docs/CURRENT-STATE.md` and `docs/TEST-MATRIX.md`.

This directory is the machine-readable knowledge layer for the Control Panel development
harness. It records implemented controls, Scaleform assets, native menu surfaces, protocols,
subscriber examples, limitations, provenance, and confidence separately from narrative docs.

The local read-only MCP server exposes:

- `catalog_list`
- `catalog_get`
- `catalog_search`
- `catalog_validate_subscriber`
- the complete `absolute-control-panel://catalog/v1` resource

Start it with a Python 3 interpreter and `catalog/mcp_server.py` as the stdio server command.
No package installation or network connection is required. The server deliberately performs no
writes; catalogue expansion remains a reviewed repository change with a stable ID, confidence,
and provenance.

Before using a changed catalogue, run `python catalog/validate_catalog.py`. A harness-generated
subscriber manifest should also pass `catalog_validate_subscriber` before any C++ is produced.
