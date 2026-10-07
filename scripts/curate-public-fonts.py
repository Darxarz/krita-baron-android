"""Download curated, unmodified OFL/Apache font faces for the public Android APK.

Build tool only: pip install fonttools pillow. The application needs neither package.
"""

import argparse
import concurrent.futures
import hashlib
import html
import json
import re
import time
import urllib.parse
import urllib.request
from pathlib import Path

from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont

REVISION = "9710da1eacb3be272583c3224dcb70f9da6eadbb"
REPOSITORY = "https://github.com/google/fonts"
RAW = f"https://raw.githubusercontent.com/google/fonts/{REVISION}/"
CURATION = {
    "Horror / distressed": "creepster eater nosifer butcherman metalmania frijole rubikwetpaint rubikbeastly",
    "Fantasy / ancient": "medievalsharp uncialantiqua almendrasc cinzeldecorative imfellenglishsc macondo jimnightshade tradewinds caesardressing pirataone newrocker kurale",
    "Blackletter": "unifrakturcook unifrakturmaguntia grenzegotisch jacquard12",
    "Ornate script": "lavishlyyours tangerine alexbrush greatvibes italianno montecarlo pinyonscript mrdafoe mrssaintdelafield meaculpa windsong lobster pacifico pattaya",
    "Vintage / circus / art deco": "rye sancreek ewert smokum frederickathegreat ribeye limelight fascinateinline monoton vastshadow bungeeinline bungeeshade",
    "Brush / handwriting": "permanentmarker rocksalt sedgwickave sedgwickavedisplay kalam caveat marckscript badscript pangolin neucha amaticsc underdog",
    "Comic / playful": "bangers boogaloo luckiestguy chewy gloriahallelujah patrickhand irishgrover",
    "Stencil / industrial": "allertastencil stardosstencil blackopsone sirinstencil sairastencilone rubikdirt",
    "Tech / pixel": "orbitron audiowide wallpoet electrolize pressstart2p vt323 silkscreen dotgothic16 rubikglitch",
    "Expressive Cyrillic display": "rubikmoonrocks rubikvinyl poiretone russoone comfortaa philosopher yesevaone",
    "Editorial / book contrast": "prata playfairdisplay forum cormorantgaramond literata",
}
RUSSIAN = "АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдеёжзийклмнопрстуфхцчшщъыьэюя"


def fetch(url):
    for attempt in range(4):
        try:
            request = urllib.request.Request(
                url, headers={"User-Agent": "Krita-Baron-font-curation/1"}
            )
            with urllib.request.urlopen(request, timeout=90) as response:
                return response.read()
        except OSError:
            if attempt == 3:
                raise
            time.sleep(attempt + 1)


def font_name(font, identifier):
    return next(
        (record.toUnicode() for record in font["name"].names if record.nameID == identifier), ""
    )


def original(path, members, cached=None):
    if cached and cached.exists():
        data = cached.read_bytes()
        digest = hashlib.sha1(f"blob {len(data)}\0".encode() + data).hexdigest()
        if digest == members[path]["sha"]:
            return data
    data = fetch(RAW + urllib.parse.quote(path))
    digest = hashlib.sha1(f"blob {len(data)}\0".encode() + data).hexdigest()
    if digest != members[path]["sha"]:
        raise ValueError(f"Upstream Git blob mismatch: {path}")
    return data


def create_family(tree, destination, category, slug):
    is_apache = any(row["path"] == f"apache/{slug}/LICENSE.txt" for row in tree)
    prefix = f"{'apache' if is_apache else 'ofl'}/{slug}/"
    members = {row["path"]: row for row in tree if row["path"].startswith(prefix)}
    candidates = [
        path for path in members if path.endswith((".ttf", ".otf")) and path.count("/") == 2
    ]
    if not candidates:
        raise ValueError(f"No original font binary for {slug}")
    # One regular static face where present; otherwise one normal variable original.
    candidates.sort(
        key=lambda path: (
            "italic" in path.lower(),
            "-regular." not in path.lower(),
            "[" not in path,
            len(path),
            path,
        )
    )
    source = candidates[0]
    licence_name = "LICENSE.txt" if is_apache else "OFL.txt"
    licence_path = prefix + licence_name
    if licence_path not in members:
        raise ValueError(f"No original license notice for {slug}")
    notice = destination / "licenses" / slug / licence_name
    licence = original(licence_path, members, notice)
    licence_text = licence.decode("utf-8-sig")
    recognized = (
        ("Apache License" in licence_text and "Version 2.0" in licence_text)
        if is_apache
        else ("SIL OPEN FONT LICENSE" in licence_text and "Version 1.1" in licence_text)
    )
    if not recognized:
        raise ValueError(f"Unrecognized license: {slug}")
    target = destination / Path(source).name
    data = original(source, members, target)
    # Verify the downloaded original against Git's SHA-1 blob object, not just self-hash.
    git_hash = hashlib.sha1(f"blob {len(data)}\0".encode() + data).hexdigest()
    if git_hash != members[source]["sha"]:
        raise ValueError(f"Upstream Git blob mismatch: {source}")
    target.write_bytes(data)
    notice.parent.mkdir(parents=True, exist_ok=True)
    notice.write_bytes(licence)
    metadata_path = notice.parent / "METADATA.pb"
    metadata_data = original(prefix + "METADATA.pb", members, metadata_path)
    metadata_path.write_bytes(metadata_data)
    metadata = metadata_data.decode("utf-8")
    for additional in ("NOTICE", "NOTICE.txt", "AUTHORS.txt", "upstream_info.md"):
        if prefix + additional in members:
            additional_path = notice.parent / additional
            additional_path.write_bytes(original(prefix + additional, members, additional_path))
    with TTFont(target) as font:
        cmap = font.getBestCmap()
        russian_missing = [
            character for character in RUSSIAN if cmap.get(ord(character), ".notdef") == ".notdef"
        ]
        family = re.search(r'^name:\s*"([^"]+)"', metadata, re.M).group(1)
        record = {
            "file": target.name,
            "bytes": len(data),
            "sha256": hashlib.sha256(data).hexdigest(),
            "kind": target.suffix.lower(),
            "source": source,
            "family": family,
            "category": category,
            "license": "Apache-2.0" if is_apache else "OFL-1.1",
            "source_url": f"{REPOSITORY}/blob/{REVISION}/{source}",
            "license_file": notice.relative_to(destination).as_posix(),
            "license_url": f"{REPOSITORY}/blob/{REVISION}/{licence_path}",
            "license_sha256": hashlib.sha256(licence).hexdigest(),
            "copyright": font_name(font, 0),
            "font_family": font_name(font, 1),
            "font_subfamily": font_name(font, 2),
            "postscript_name": font_name(font, 6),
            "variable": "fvar" in font,
            "cyrillic": not russian_missing,
            "cyrillic_basis": "All 66 Russian upper/lower letters including Yo must have cmap entries",
            "russian_missing": "".join(russian_missing),
            "glyphs": len(cmap),
            "upstream_git_blob_sha1": git_hash,
        }
        (notice.parent / "COPYRIGHT.txt").write_text(record["copyright"] + "\n", encoding="utf-8")
    # FreeType must parse and rasterize each actual binary, including variable faces.
    ImageFont.truetype(str(target), 32).getmask("Baron 123")
    return record


def catalog(destination, manifest):
    records = manifest["files"]
    rows = []
    index = [
        "<!doctype html><html lang='ru'><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Baron expressive fonts</title>",
        "<style>body{background:#181a1e;color:#e7e9ee;font:16px sans-serif;padding:30px}"
        "section{background:#24272d;padding:18px;margin:12px;border-radius:12px}"
        ".sample{font-size:42px;line-height:1.7;overflow-wrap:anywhere}small{color:#abb0bd}"
        "a{color:#9ebcff}input,select{padding:10px;background:#24272d;color:#fff;border:1px solid #535966;border-radius:8px}"
        "nav{position:sticky;top:0;background:#181a1e;padding:12px 0;display:flex;gap:12px;flex-wrap:wrap}"
        "@media(max-width:600px){body{padding:12px}.sample{font-size:34px}section{margin:12px 0}}</style>",
        f"<h1>{len(records)} expressive font families</h1><p>Original OFL/Apache fonts. One face per family. "
        "All may be bundled with apps and used for commercial print. Russian support is checked from glyphs.</p>",
        "<nav><input id='search' type='search' placeholder='Поиск по имени и стилю'><select id='category'><option value=''>Все стили</option>"
        + "".join(f"<option>{html.escape(category)}</option>" for category in CURATION)
        + "</select><label><input type='checkbox' id='russian'> С русским алфавитом</label><span id='count'></span></nav>",
    ]
    for i, row in enumerate(records):
        safe_file = urllib.parse.quote(row["file"])
        family = html.escape(row["family"])
        label = "Russian: full alphabet" if row["cyrillic"] else "Latin (Russian incomplete)"
        index.append(
            f"<style>@font-face{{font-family:f{i};src:url('{safe_file}')}}</style>"
            f"<section data-family='{family}' data-category='{html.escape(row['category'])}' data-ru='{str(row['cyrillic']).lower()}'><b>{family}</b> <small>{html.escape(row['category'])} · {label}</small>"
            f"<div class='sample' style='font-family:f{i}'>Baron · Magic &amp; Mystery 123</div>"
            + (
                f"<div class='sample' style='font-family:f{i}'>Волшебство · Тайна и красота</div>"
                if row["cyrillic"]
                else ""
            )
            + f"<a href='{html.escape(row['license_file'])}'>Original license</a></section>"
        )
        rows.append(row)
    index.append(
        "<script>function filter(){let q=document.querySelector('#search').value.toLowerCase(),"
        "c=document.querySelector('#category').value,r=document.querySelector('#russian').checked,n=0;"
        "document.querySelectorAll('section').forEach(s=>{s.hidden=!(s.textContent.toLowerCase().includes(q)&&"
        "(!c||s.dataset.category===c)&&(!r||s.dataset.ru==='true'));if(!s.hidden)n++});"
        "document.querySelector('#count').textContent=n+' / '+document.querySelectorAll('section').length;}"
        "document.querySelectorAll('nav input,nav select').forEach(e=>e.addEventListener('input',filter));filter();</script>"
    )
    (destination / "catalog.html").write_text("\n".join(index) + "</html>", encoding="utf-8")
    heading = (
        ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 20)
        if Path("C:/Windows/Fonts/segoeui.ttf").exists()
        else ImageFont.load_default()
    )
    for page in range((len(rows) + 19) // 20):
        image = Image.new("RGB", (1600, 1450), "#181a1e")
        draw = ImageDraw.Draw(image)
        draw.text(
            (28, 14),
            f"Krita Baron Edition - expressive fonts / {page + 1}",
            font=heading,
            fill="#e7e9ee",
        )
        for offset, row in enumerate(rows[page * 20 : (page + 1) * 20]):
            x = 28 + (offset % 2) * 790
            y = 62 + (offset // 2) * 138
            draw.rounded_rectangle((x - 8, y - 6, x + 753, y + 119), radius=9, fill="#24272d")
            draw.text(
                (x, y),
                row["family"] + (" / RU" if row["cyrillic"] else " / Latin"),
                font=heading,
                fill="#aeb6c6",
            )
            sample = "Тайна и красота" if row["cyrillic"] else "Magic & Mystery"
            for size in range(46, 19, -1):
                face = ImageFont.truetype(str(destination / row["file"]), size)
                box = face.getbbox(sample)
                if box[2] - box[0] <= 747 and box[3] - box[1] <= 63:
                    break
            draw.text((x, y + 34), sample, font=face, fill="#f4e8ce", anchor="lt")
            draw.text((x, y + 99), row["category"], font=heading, fill="#78859a")
        image.save(destination / f"catalog-{page + 1:02}.png")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", type=Path, default=Path("build/curated-fonts"))
    parser.add_argument("--tree", type=Path)
    parser.add_argument(
        "--verify-only", action="store_true", help="Validate an existing bundle offline"
    )
    args = parser.parse_args()
    destination = args.destination.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    if args.verify_only:
        manifest = json.loads((destination / "manifest.json").read_text(encoding="utf-8"))
        records = manifest["files"]
        if not manifest.get("public_bundle") or manifest.get("personal_bundle"):
            raise ValueError("Not a public font bundle")
        for row in records:
            data = (destination / row["file"]).read_bytes()
            licence = (destination / row["license_file"]).read_bytes()
            if hashlib.sha256(data).hexdigest() != row["sha256"] or len(data) != row["bytes"]:
                raise ValueError(f"Damaged font: {row['file']}")
            if hashlib.sha256(licence).hexdigest() != row["license_sha256"]:
                raise ValueError(f"Damaged original license: {row['family']}")
            git_hash = hashlib.sha1(f"blob {len(data)}\0".encode() + data).hexdigest()
            if git_hash != row["upstream_git_blob_sha1"]:
                raise ValueError(f"Modified original font: {row['family']}")
            with TTFont(destination / row["file"]) as font:
                cmap = font.getBestCmap()
                cyrillic = all(
                    cmap.get(ord(character), ".notdef") != ".notdef" for character in RUSSIAN
                )
                if cyrillic != row["cyrillic"]:
                    raise ValueError(f"Glyph coverage mismatch: {row['family']}")
            ImageFont.truetype(str(destination / row["file"]), 32).getmask("Baron 123")
        if sum(row["bytes"] for row in records) != manifest["bytes"]:
            raise ValueError("Bundle byte count mismatch")
        print(
            json.dumps(
                {
                    "verified_families": len(records),
                    "bytes": manifest["bytes"],
                    "cyrillic_families": sum(row["cyrillic"] for row in records),
                    "original_hashes_licenses_and_rasterization": True,
                }
            )
        )
        return
    tree_data = (
        json.loads(args.tree.read_text())
        if args.tree
        else json.loads(
            fetch(f"https://api.github.com/repos/google/fonts/git/trees/{REVISION}?recursive=1")
        )
    )
    if tree_data["sha"] != REVISION or tree_data.get("truncated"):
        raise ValueError("Expected complete tree at pinned upstream revision")
    families = [(category, slug) for category, names in CURATION.items() for slug in names.split()]
    if len({slug for _, slug in families}) != len(families):
        raise ValueError("Duplicate family")
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as executor:
        futures = [
            executor.submit(create_family, tree_data["tree"], destination, category, slug)
            for category, slug in families
        ]
        records = [future.result() for future in futures]
    manifest = {
        "schema": 1,
        "personal_bundle": False,
        "public_bundle": True,
        "title": "Baron expressive fonts",
        "upstream_revision": REVISION,
        "repository": REPOSITORY,
        "files": records,
        "families": len(records),
        "bytes": sum(row["bytes"] for row in records),
        "licensing_reference": "https://openfontlicense.org/ofl-faq/",
        "cyrillic_families": sum(row["cyrillic"] for row in records),
    }
    (destination / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    catalog(destination, manifest)
    print(
        json.dumps(
            {
                key: manifest[key]
                for key in ("families", "bytes", "cyrillic_families", "upstream_revision")
            }
        )
    )


if __name__ == "__main__":
    main()
