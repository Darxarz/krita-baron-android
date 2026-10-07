"""Prepare an inspectable site patch without modifying the site's files."""

import argparse
import difflib
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("site", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--review-directory", type=Path)
args = parser.parse_args()
changes = []


def replace(text, old, new):
    if text.count(old) != 1:
        raise SystemExit(f"Expected one patch location, found {text.count(old)}: {old[:100]}")
    return text.replace(old, new)


route = "routes/kritaIntegration.js"
before = (args.site / route).read_text(encoding="utf-8")
after = replace(
    before,
    "  router.post('/api/krita/native/prepare'",
    """  router.get('/api/krita/model-metadata', wrap(async (req, res) => {
    const user = await userFor(req);
    const name = req.query?.name;
    const kind = req.query?.kind;
    if (typeof name !== 'string' || !name || name.length > 2048 || !['checkpoint', 'lora'].includes(kind)) {
      return res.status(400).json({ error: 'Invalid model' });
    }
    const { visibleModel } = require('../services/kritaCatalogDetails');
    const model = visibleModel(await getModels(user, { detailsFor: { name, kind } }), name, kind);
    if (!model) return res.status(404).json({ error: 'Model is unavailable for this connection' });
    res.json({ success: true, data: model });
  }));
  router.post('/api/krita/native/prepare'""",
)
changes.append((route, before, after))
portal = "userPortalServer.js"
before = (args.site / portal).read_text(encoding="utf-8")
after = replace(
    before,
    "    getModels: async (user) => {",
    "    getModels: async (user, { detailsFor } = {}) => {",
)
after = replace(
    after,
    "['fileName', 'name', 'baseModel', 'imageUrl', 'thumbnailUrl', 'trainedWordsJson']",
    "['fileName', 'name', 'baseModel', 'imageUrl', 'thumbnailUrl', 'trainedWordsJson', 'description', 'tagsJson', 'creator', 'versionName', 'modelUrl']",
)
after = replace(
    after,
    "return { ...model, title: sidecar?.displayName",
    "return { ...model, ...require('./services/kritaCatalogDetails').catalogDetails(sidecar, entry, detailsFor && detailsFor.name.replace(/\\\\/g, '/') === normalized && (detailsFor.kind === 'lora' ? model.kind === 'lora' : model.kind !== 'lora')), title: sidecar?.displayName",
)
changes.append((portal, before, after))
for name in ("services/kritaModelMetadata.js", "services/loraMetadataService.js"):
    before = (args.site / name).read_text(encoding="utf-8")
    after = replace(
        before,
        "  return {\n    displayName:",
        "  return {\n    ...require('./kritaCatalogDetails').catalogDetails(metadata, {}, true),\n    displayName:",
    )
    changes.append((name, before, after))
helper = "services/kritaCatalogDetails.js"
changes.append(
    (helper, "", Path(__file__).with_name("kritaCatalogDetails.js").read_text(encoding="utf-8"))
)
args.output.parent.mkdir(parents=True, exist_ok=True)
with args.output.open("w", encoding="utf-8", newline="\n") as output:
    for name, old, new in changes:
        output.writelines(
            difflib.unified_diff(
                old.splitlines(True),
                new.splitlines(True),
                fromfile="a/" + name if old else "/dev/null",
                tofile="b/" + name,
            )
        )
print(args.output)
if args.review_directory:
    for name, _, new in changes:
        target = args.review_directory / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(new, encoding="utf-8")
