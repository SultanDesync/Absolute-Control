import json
import pathlib
import re
import sys


ROOT = pathlib.Path(__file__).resolve().parent
CATALOG = ROOT / "catalog.json"


def validate(data):
    errors = []
    if data.get("schemaVersion") != 1:
        errors.append("schemaVersion must be 1")
    if data.get("product") != "Absolute Control Panel":
        errors.append("unexpected product name")
    entries = data.get("entries")
    if not isinstance(entries, list):
        return errors + ["entries must be an array"]
    seen = set()
    pattern = re.compile(r"^[a-z0-9][a-z0-9.-]+$")
    for index, entry in enumerate(entries):
        if not isinstance(entry, dict):
            errors.append(f"entries[{index}] must be an object")
            continue
        for field in ("id", "kind", "name", "status", "confidence", "tags"):
            if field not in entry:
                errors.append(f"entries[{index}] missing {field}")
        identifier = entry.get("id", "")
        if not pattern.fullmatch(identifier):
            errors.append(f"entries[{index}] has invalid id {identifier!r}")
        if identifier in seen:
            errors.append(f"duplicate id {identifier}")
        seen.add(identifier)
        if not isinstance(entry.get("tags"), list):
            errors.append(f"{identifier or index} tags must be an array")
    return errors


def main():
    data = json.loads(CATALOG.read_text(encoding="utf-8"))
    errors = validate(data)
    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1
    print(f"validated {len(data['entries'])} catalogue entries")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
