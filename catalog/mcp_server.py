"""Read-only stdio MCP server for the Absolute Control Panel catalogue."""

import json
import pathlib
import sys


ROOT = pathlib.Path(__file__).resolve().parent
CATALOG_PATH = ROOT / "catalog.json"
CATALOG_URI = "absolute-control-panel://catalog/v1"


def load_catalog():
    return json.loads(CATALOG_PATH.read_text(encoding="utf-8"))


def text_result(value, is_error=False):
    return {
        "content": [{"type": "text", "text": json.dumps(value, indent=2)}],
        "isError": is_error,
    }


def tools_list():
    return {
        "tools": [
            {
                "name": "catalog_list",
                "description": "List catalogue entries, optionally filtered by kind or tag.",
                "inputSchema": {
                    "type": "object",
                    "properties": {
                        "kind": {"type": "string"},
                        "tag": {"type": "string"},
                    },
                },
            },
            {
                "name": "catalog_get",
                "description": "Get one catalogue entry by stable id.",
                "inputSchema": {
                    "type": "object",
                    "required": ["id"],
                    "properties": {"id": {"type": "string"}},
                },
            },
            {
                "name": "catalog_search",
                "description": "Search names, ids, notes, tags, and structured entry fields.",
                "inputSchema": {
                    "type": "object",
                    "required": ["query"],
                    "properties": {"query": {"type": "string", "minLength": 1}},
                },
            },
            {
                "name": "catalog_validate_subscriber",
                "description": "Validate a proposed subscriber page/control manifest against current host capacities and implemented control support.",
                "inputSchema": {
                    "type": "object",
                    "required": ["moduleId", "pages"],
                    "properties": {
                        "moduleId": {"type": "string"},
                        "pages": {
                            "type": "array",
                            "items": {
                                "type": "object",
                                "required": ["pageId", "controls"],
                                "properties": {
                                    "pageId": {"type": "string"},
                                    "controls": {
                                        "type": "array",
                                        "items": {
                                            "type": "object",
                                            "required": ["controlId", "kind"],
                                            "properties": {
                                                "controlId": {"type": "string"},
                                                "kind": {"type": "string"},
                                            },
                                        },
                                    },
                                },
                            },
                        },
                    },
                },
            },
        ]
    }


def call_tool(name, arguments):
    catalog = load_catalog()
    entries = catalog["entries"]
    if name == "catalog_list":
        kind = arguments.get("kind")
        tag = arguments.get("tag")
        matches = [entry for entry in entries
                   if (not kind or entry["kind"] == kind)
                   and (not tag or tag in entry.get("tags", []))]
        return text_result(matches)
    if name == "catalog_get":
        match = next((entry for entry in entries if entry["id"] == arguments.get("id")), None)
        return text_result(match or {"error": "entry not found"}, match is None)
    if name == "catalog_search":
        query = arguments.get("query", "").casefold()
        matches = [entry for entry in entries
                   if query in json.dumps(entry, sort_keys=True).casefold()]
        return text_result(matches)
    if name == "catalog_validate_subscriber":
        return text_result(validate_subscriber(arguments))
    return text_result({"error": f"unknown tool {name}"}, True)


def validate_subscriber(manifest):
    errors = []
    warnings = []
    module_id = manifest.get("moduleId", "")
    pages = manifest.get("pages", [])
    supported = {"Toggle", "IntegerSlider", "FloatSlider", "Action", "InputBinding"}
    if not module_id or len(module_id) >= 64:
        errors.append("moduleId must contain 1-63 characters")
    if len(pages) > 32:
        errors.append("host model accepts at most 32 total pages")
    total = 0
    page_ids = set()
    for page in pages:
        page_id = page.get("pageId", "")
        controls = page.get("controls", [])
        if not page_id or len(page_id) >= 64:
            errors.append("each pageId must contain 1-63 characters")
        if page_id in page_ids:
            errors.append(f"duplicate pageId {page_id}")
        page_ids.add(page_id)
        if len(controls) > 128:
            errors.append(f"page {page_id} exceeds 128 controls")
        total += len(controls)
        control_ids = set()
        for control in controls:
            control_id = control.get("controlId", "")
            kind = control.get("kind", "")
            if not control_id or len(control_id) >= 64:
                errors.append(f"page {page_id} has an invalid controlId")
            if control_id in control_ids:
                errors.append(f"page {page_id} duplicates controlId {control_id}")
            control_ids.add(control_id)
            if kind == "Choice":
                warnings.append(f"{page_id}/{control_id}: Choice is not rendered yet")
            elif kind not in supported:
                errors.append(f"{page_id}/{control_id}: unsupported kind {kind}")
    if total > 512:
        errors.append("one subscriber module accepts at most 512 controls")
    return {"valid": not errors, "errors": errors, "warnings": warnings,
            "pageCount": len(pages), "controlCount": total}


def dispatch(message):
    method = message.get("method")
    if method == "initialize":
        return {
            "protocolVersion": "2025-06-18",
            "capabilities": {"tools": {}, "resources": {}},
            "serverInfo": {"name": "absolute-control-panel-catalog", "version": "0.1.0"},
        }
    if method == "ping":
        return {}
    if method == "tools/list":
        return tools_list()
    if method == "tools/call":
        params = message.get("params", {})
        return call_tool(params.get("name"), params.get("arguments") or {})
    if method == "resources/list":
        return {"resources": [{"uri": CATALOG_URI, "name": "Absolute Control Panel catalogue",
                                "mimeType": "application/json"}]}
    if method == "resources/read":
        if message.get("params", {}).get("uri") != CATALOG_URI:
            raise ValueError("unknown resource")
        return {"contents": [{"uri": CATALOG_URI, "mimeType": "application/json",
                              "text": json.dumps(load_catalog(), indent=2)}]}
    if method and method.startswith("notifications/"):
        return None
    raise ValueError(f"unsupported method {method}")


def main():
    for line in sys.stdin:
        if not line.strip():
            continue
        message = json.loads(line)
        identifier = message.get("id")
        try:
            result = dispatch(message)
            if identifier is None or result is None:
                continue
            response = {"jsonrpc": "2.0", "id": identifier, "result": result}
        except Exception as error:
            if identifier is None:
                continue
            response = {"jsonrpc": "2.0", "id": identifier,
                        "error": {"code": -32603, "message": str(error)}}
        sys.stdout.write(json.dumps(response, separators=(",", ":")) + "\n")
        sys.stdout.flush()


if __name__ == "__main__":
    main()
