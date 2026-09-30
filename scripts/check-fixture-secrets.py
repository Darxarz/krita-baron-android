import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
info = json.loads((root / "tests/fixtures/object_info.json").read_text(encoding="utf-8"))
suspect = []
for node, metadata in info.items():
    for group in metadata.get("input", {}).values():
        if not isinstance(group, dict):
            continue
        for field, definition in group.items():
            if not any(
                word in field.lower() for word in ("password", "api_key", "secret", "access_token")
            ):
                continue
            if (
                isinstance(definition, list)
                and len(definition) > 1
                and isinstance(definition[1], dict)
            ):
                default = definition[1].get("default")
                if isinstance(default, str) and default.strip():
                    suspect.append(f"{node}.{field}")
if suspect:
    raise SystemExit("Review nonempty credential defaults before publishing: " + ", ".join(suspect))
print("No nonempty credential defaults in the ComfyUI fixture")
