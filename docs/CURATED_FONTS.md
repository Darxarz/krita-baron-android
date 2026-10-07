# Baron expressive fonts

The public Android font bundle contains **96 genuinely different families**, one original
TTF per family, **20,765,564 bytes** of font data. There are **30 families with the full
Russian alphabet**, including Ё/ё. The other families should be treated as Latin display
fonts; Krita may substitute a fallback for unsupported letters.

The collection deliberately emphasizes horror, fantasy/ancient lettering, blackletter,
ornate script, vintage/circus/art deco, brush lettering, comics, stencil, and pixel/tech.
A small set of contrasting book/display fonts provides readable alternatives. Weight
variants are not counted as extra families. This is a curated artistic collection rather
than an exhaustive Google Fonts download.

For the Chiller direction, try **Creepster, Eater, Butcherman, Metal Mania**, or the
Cyrillic **Rubik Wet Paint / Rubik Beastly**. For ancient/fantasy lettering, try
**Uncial Antiqua, MedievalSharp, Macondo, Almendra SC**, or Cyrillic **Kurale**.
These are independent creative alternatives, not reproductions of Chiller or Papyrus.
For Cyrillic flourishes, try **Great Vibes, Lobster, Pacifico, Pattaya, Marck Script**.

## Files and reproducibility

- `scripts/curate-public-fonts.py` is the reproducible curated build helper.
- `build/curated-fonts/manifest.json` records every family, category, original source URL,
  license URL, font/license SHA-256, upstream Git blob hash, byte count, and glyph support.
- `build/curated-fonts/licenses/<family>/` contains the original license, copyright,
  Google Fonts metadata, and available upstream attribution information.
- `build/curated-fonts/catalog.html` displays the actual font binaries and offers search,
  category filters, and a Russian-alphabet filter. Keep it next to the TTF and license
  files when sharing this catalogue. `catalog-01.png` through `catalog-05.png` are static
  sheets rendered with the actual fonts; no imitated/generated lettering is used.

The official Google Fonts repository is pinned to commit
[`9710da1eacb3be272583c3224dcb70f9da6eadbb`](https://github.com/google/fonts/tree/9710da1eacb3be272583c3224dcb70f9da6eadbb).
All font binaries are original and unmodified: no conversion, subsetting, glyph edits,
variable-font instancing, or internal family renaming. Each original font and license
is checked against that commit's Git blob SHA-1 and recorded with SHA-256.

Use a separate build environment with `fonttools` and `pillow`; these are build-only
dependencies and do not enter the application:

```powershell
python -m venv build/font-tools
build/font-tools/Scripts/python -m pip install fonttools pillow
build/font-tools/Scripts/python scripts/curate-public-fonts.py
build/font-tools/Scripts/python scripts/curate-public-fonts.py --verify-only
```

The destination is ignored build output. For APK packaging, include the 96 TTF files,
`manifest.json`, and the complete `licenses` directory. PNG/HTML catalogue files and
desktop verification reports may remain a separate download. Do not mix this public
bundle with `build/personal-fonts` or a Windows font collection.

## License evidence

**90 families use SIL OFL 1.1; six use Apache 2.0** (Smokum, Permanent Marker, Rock Salt,
Luckiest Guy, Chewy, Irish Grover). The manifest links to each family's actual license.
Embedding flags are not used as a substitute for licensing permission.

The [official OFL FAQ](https://openfontlicense.org/ofl-faq/) explicitly allows commercial
design/print (1.1), software bundles (1.4), and mobile application bundling (1.20). Keep
the copyright, license notice, and license text with each redistributed font. OFL font
files retain their license; the resulting poster, drawing, print, or other artwork does
not acquire the font's license. Fonts must not be sold on their own. Reserved Font Names
remain untouched because these binaries are not modified.

The [Apache 2.0 license](https://www.apache.org/licenses/LICENSE-2.0) grants reproduction,
public display, and distribution and requires redistribution of the license and relevant
notices. The bundle preserves each Apache font's original license and embedded copyright,
and copies any available NOTICE/author files. Neither license implies that its authors
endorse Krita Baron Edition.

Representative original license files:

- [Creepster OFL](https://github.com/google/fonts/blob/9710da1eacb3be272583c3224dcb70f9da6eadbb/ofl/creepster/OFL.txt)
- [Uncial Antiqua OFL](https://github.com/google/fonts/blob/9710da1eacb3be272583c3224dcb70f9da6eadbb/ofl/uncialantiqua/OFL.txt)
- [Great Vibes OFL](https://github.com/google/fonts/blob/9710da1eacb3be272583c3224dcb70f9da6eadbb/ofl/greatvibes/OFL.txt)
- [Permanent Marker Apache](https://github.com/google/fonts/blob/9710da1eacb3be272583c3224dcb70f9da6eadbb/apache/permanentmarker/LICENSE.txt)

## Verification and limits

- Font SHA-256/byte counts and original Git blob hashes: **96/96 passed**.
- Original license SHA-256 and strict cmap checks: **96/96 passed**.
- FreeType parsing/rasterization using the actual binaries: **96/96 passed**.
- Desktop Fontconfig discovery: **96/96 recognized**, no unsupported files.
- Desktop Qt 5.15 `QFontDatabase.addApplicationFont`: **96/96 registered**.
- Ruff check and format: passed for the helper.

The manifest's `cyrillic: true` means every one of the 66 Russian upper/lowercase letters
has a real cmap glyph, including Ё/ё. It does not claim all Slavic alphabets or every
Unicode symbol. Desktop discovery is not physical Samsung/Android text-tool acceptance;
the packaged APK still needs that check. Variable faces use their original default axes
unless the application's text controls choose other values.

## Family inventory

| Family | Style | Russian | License |
|---|---|---|---|
| Creepster | Horror / distressed | Incomplete | OFL-1.1 |
| Eater | Horror / distressed | Incomplete | OFL-1.1 |
| Nosifer | Horror / distressed | Incomplete | OFL-1.1 |
| Butcherman | Horror / distressed | Incomplete | OFL-1.1 |
| Metal Mania | Horror / distressed | Incomplete | OFL-1.1 |
| Frijole | Horror / distressed | Incomplete | OFL-1.1 |
| Rubik Wet Paint | Horror / distressed | Yes | OFL-1.1 |
| Rubik Beastly | Horror / distressed | Yes | OFL-1.1 |
| MedievalSharp | Fantasy / ancient | Incomplete | OFL-1.1 |
| Uncial Antiqua | Fantasy / ancient | Incomplete | OFL-1.1 |
| Almendra SC | Fantasy / ancient | Incomplete | OFL-1.1 |
| Cinzel Decorative | Fantasy / ancient | Incomplete | OFL-1.1 |
| IM Fell English SC | Fantasy / ancient | Incomplete | OFL-1.1 |
| Macondo | Fantasy / ancient | Incomplete | OFL-1.1 |
| Jim Nightshade | Fantasy / ancient | Incomplete | OFL-1.1 |
| Trade Winds | Fantasy / ancient | Incomplete | OFL-1.1 |
| Caesar Dressing | Fantasy / ancient | Incomplete | OFL-1.1 |
| Pirata One | Fantasy / ancient | Incomplete | OFL-1.1 |
| New Rocker | Fantasy / ancient | Incomplete | OFL-1.1 |
| Kurale | Fantasy / ancient | Yes | OFL-1.1 |
| UnifrakturCook | Blackletter | Incomplete | OFL-1.1 |
| UnifrakturMaguntia | Blackletter | Incomplete | OFL-1.1 |
| Grenze Gotisch | Blackletter | Incomplete | OFL-1.1 |
| Jacquard 12 | Blackletter | Incomplete | OFL-1.1 |
| Lavishly Yours | Ornate script | Incomplete | OFL-1.1 |
| Tangerine | Ornate script | Incomplete | OFL-1.1 |
| Alex Brush | Ornate script | Incomplete | OFL-1.1 |
| Great Vibes | Ornate script | Yes | OFL-1.1 |
| Italianno | Ornate script | Incomplete | OFL-1.1 |
| MonteCarlo | Ornate script | Incomplete | OFL-1.1 |
| Pinyon Script | Ornate script | Incomplete | OFL-1.1 |
| Mr Dafoe | Ornate script | Incomplete | OFL-1.1 |
| Mrs Saint Delafield | Ornate script | Incomplete | OFL-1.1 |
| Mea Culpa | Ornate script | Incomplete | OFL-1.1 |
| WindSong | Ornate script | Incomplete | OFL-1.1 |
| Lobster | Ornate script | Yes | OFL-1.1 |
| Pacifico | Ornate script | Yes | OFL-1.1 |
| Pattaya | Ornate script | Yes | OFL-1.1 |
| Rye | Vintage / circus / art deco | Incomplete | OFL-1.1 |
| Sancreek | Vintage / circus / art deco | Incomplete | OFL-1.1 |
| Ewert | Vintage / circus / art deco | Incomplete | OFL-1.1 |
| Smokum | Vintage / circus / art deco | Incomplete | Apache-2.0 |
| Fredericka the Great | Vintage / circus / art deco | Incomplete | OFL-1.1 |
| Ribeye | Vintage / circus / art deco | Incomplete | OFL-1.1 |
| Limelight | Vintage / circus / art deco | Incomplete | OFL-1.1 |
| Fascinate Inline | Vintage / circus / art deco | Incomplete | OFL-1.1 |
| Monoton | Vintage / circus / art deco | Incomplete | OFL-1.1 |
| Vast Shadow | Vintage / circus / art deco | Incomplete | OFL-1.1 |
| Bungee Inline | Vintage / circus / art deco | Incomplete | OFL-1.1 |
| Bungee Shade | Vintage / circus / art deco | Incomplete | OFL-1.1 |
| Permanent Marker | Brush / handwriting | Incomplete | Apache-2.0 |
| Rock Salt | Brush / handwriting | Incomplete | Apache-2.0 |
| Sedgwick Ave | Brush / handwriting | Incomplete | OFL-1.1 |
| Sedgwick Ave Display | Brush / handwriting | Incomplete | OFL-1.1 |
| Kalam | Brush / handwriting | Incomplete | OFL-1.1 |
| Caveat | Brush / handwriting | Yes | OFL-1.1 |
| Marck Script | Brush / handwriting | Yes | OFL-1.1 |
| Bad Script | Brush / handwriting | Yes | OFL-1.1 |
| Pangolin | Brush / handwriting | Yes | OFL-1.1 |
| Neucha | Brush / handwriting | Yes | OFL-1.1 |
| Amatic SC | Brush / handwriting | Yes | OFL-1.1 |
| Underdog | Brush / handwriting | Yes | OFL-1.1 |
| Bangers | Comic / playful | Incomplete | OFL-1.1 |
| Boogaloo | Comic / playful | Incomplete | OFL-1.1 |
| Luckiest Guy | Comic / playful | Incomplete | Apache-2.0 |
| Chewy | Comic / playful | Incomplete | Apache-2.0 |
| Gloria Hallelujah | Comic / playful | Incomplete | OFL-1.1 |
| Patrick Hand | Comic / playful | Incomplete | OFL-1.1 |
| Irish Grover | Comic / playful | Incomplete | Apache-2.0 |
| Allerta Stencil | Stencil / industrial | Incomplete | OFL-1.1 |
| Stardos Stencil | Stencil / industrial | Incomplete | OFL-1.1 |
| Black Ops One | Stencil / industrial | Incomplete | OFL-1.1 |
| Sirin Stencil | Stencil / industrial | Incomplete | OFL-1.1 |
| Saira Stencil One | Stencil / industrial | Incomplete | OFL-1.1 |
| Rubik Dirt | Stencil / industrial | Yes | OFL-1.1 |
| Orbitron | Tech / pixel | Incomplete | OFL-1.1 |
| Audiowide | Tech / pixel | Incomplete | OFL-1.1 |
| Wallpoet | Tech / pixel | Incomplete | OFL-1.1 |
| Electrolize | Tech / pixel | Incomplete | OFL-1.1 |
| Press Start 2P | Tech / pixel | Yes | OFL-1.1 |
| VT323 | Tech / pixel | Incomplete | OFL-1.1 |
| Silkscreen | Tech / pixel | Incomplete | OFL-1.1 |
| DotGothic16 | Tech / pixel | Yes | OFL-1.1 |
| Rubik Glitch | Tech / pixel | Yes | OFL-1.1 |
| Rubik Moonrocks | Expressive Cyrillic display | Yes | OFL-1.1 |
| Rubik Vinyl | Expressive Cyrillic display | Yes | OFL-1.1 |
| Poiret One | Expressive Cyrillic display | Yes | OFL-1.1 |
| Russo One | Expressive Cyrillic display | Yes | OFL-1.1 |
| Comfortaa | Expressive Cyrillic display | Yes | OFL-1.1 |
| Philosopher | Expressive Cyrillic display | Yes | OFL-1.1 |
| Yeseva One | Expressive Cyrillic display | Yes | OFL-1.1 |
| Prata | Editorial / book contrast | Yes | OFL-1.1 |
| Playfair Display | Editorial / book contrast | Yes | OFL-1.1 |
| Forum | Editorial / book contrast | Yes | OFL-1.1 |
| Cormorant Garamond | Editorial / book contrast | Yes | OFL-1.1 |
| Literata | Editorial / book contrast | Yes | OFL-1.1 |
