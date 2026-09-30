import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
output = ROOT / "native/language"
output.mkdir(exist_ok=True)
overrides = json.loads((ROOT / "native/ru-native.json").read_text(encoding="utf-8"))
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
for file in (ROOT / "server/vendor/ai_diffusion/language").glob("*.json"):
    data = json.loads(file.read_text(encoding="utf-8"))
    result = {}
    for key in sorted(keys):
        value = data["translations"].get(key)
        if data["id"] == "en":
            value = key
        if key in aliases:
            alias, replacements = aliases[key]
            value = data["translations"].get(alias, alias)
            for old, new in replacements.items():
                value = value.replace(old, new)
        if data["id"] == "ru":
            value = overrides.get(key, value)
        if value:
            result[key] = value
    (output / file.name).write_text(
        json.dumps(
            {"id": data["id"], "name": data["name"], "translations": result},
            ensure_ascii=False,
            indent=2,
        ),
        encoding="utf-8",
    )
    print(f"{data['id']}: {len(result)}/{len(keys)} strings")
    if data["id"] in ("en", "ru") and keys.difference(result):
        raise SystemExit("Missing translations: " + ", ".join(sorted(keys.difference(result))))
qrc = (
    '<RCC><qresource prefix="/baron">\n'
    + "".join(f"<file>language/{file.name}</file>\n" for file in sorted(output.glob("*.json")))
    + "</qresource></RCC>\n"
)
(ROOT / "native/translations.qrc").write_text(qrc, encoding="utf-8")
