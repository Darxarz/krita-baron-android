"""Add missing native UI strings to native/language/*.json.

Existing translations are never removed or changed: the catalogs also hold hand-written
translations which are not available anywhere else. For a string the catalog does not have
yet, the value comes from native/<language>-native.json (native-only strings), then from the
Python plugin catalogs in server/vendor. English uses the source text. Russian and English
must be complete.
"""

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
output = ROOT / "native/language"
vendor = ROOT / "server/vendor/ai_diffusion/language"


def read_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8")) if path.exists() else {}


keys = set()
for file in list((ROOT / "native").glob("*.cpp")) + list((ROOT / "krita").glob("*.cpp")):
    literals = re.findall(
        r'\btr\(\s*((?:"(?:[^"\\]|\\.)*"\s*)+)\)', file.read_text(encoding="utf-8")
    )
    keys.update(
        "".join(json.loads(part) for part in re.findall(r'"(?:[^"\\]|\\.)*"', literal))
        for literal in literals
    )
aliases = {
    "≈ %1 bleatbucks": ("≈ {amount} bleatbucks", {"{amount}": "%1"}),
    "%1 · %2 bleatbucks": (
        "Connected · {name}\nBalance: {amount} bleatbucks",
        {"{name}": "%1", "{amount}": "%2", "\n": " · "},
    ),
}
incomplete = []
for path in sorted(output.glob("*.json")):
    raw = path.read_bytes().decode("utf-8")
    newline = "\r\n" if "\r\n" in raw else "\n"
    data = json.loads(raw)
    language = data["id"]
    translations = data["translations"]
    native = read_json(ROOT / f"native/{language}-native.json")
    plugin = read_json(vendor / f"{language}.json").get("translations", {})
    added = 0
    for key in sorted(keys - translations.keys()):
        value = key if language == "en" else native.get(key) or plugin.get(key)
        if language != "en" and not value and key in aliases:
            alias, replacements = aliases[key]
            value = plugin.get(alias)
            for old, new in replacements.items():
                value = value.replace(old, new) if value else value
        if value:
            translations[key] = value
            added += 1
    missing = sorted(keys - translations.keys())
    if language in ("en", "ru") and missing:
        incomplete.append(f"{language}: " + ", ".join(missing))
    text = json.dumps(data, ensure_ascii=False, indent=2)
    if raw.endswith(("\n", "\r\n")):
        text += "\n"
    if text.replace("\n", newline) != raw:
        path.write_bytes(text.replace("\n", newline).encode("utf-8"))
    print(f"{language}: +{added}, {len(keys) - len(missing)}/{len(keys)} strings")
if incomplete:
    raise SystemExit("Missing translations:\n" + "\n".join(incomplete))
qrc = (
    '<RCC><qresource prefix="/baron">\n'
    + "".join(f"<file>language/{file.name}</file>\n" for file in sorted(output.glob("*.json")))
    + "</qresource></RCC>\n"
)
(ROOT / "native/translations.qrc").write_text(qrc, encoding="utf-8")
