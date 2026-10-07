// SPDX-License-Identifier: GPL-3.0-or-later
const text = value => typeof value === 'string' ? value.slice(0, 65536) : '';
function list(value) {
  if (typeof value === 'string') {
    try { value = JSON.parse(value); } catch (_) { value = value.split(','); }
  }
  return Array.isArray(value) ? [...new Set(value.filter(v => typeof v === 'string').map(v => v.trim()).filter(Boolean))].slice(0, 256) : [];
}
function catalogDetails(sidecar = {}, entry = {}, detailed = false) {
  sidecar ||= {}; entry ||= {};
  const civitai = sidecar.civitai || {};
  const creator = sidecar.creator || civitai.creator || entry.creator;
  const result = {
    tags: list(sidecar.tags || civitai.model?.tags).length ? list(sidecar.tags || civitai.model?.tags) : list(entry.tagsJson),
    creator: text(typeof creator === 'object' ? creator?.username : creator),
    versionName: text(sidecar.versionName || civitai.name || entry.versionName),
    modelUrl: text(sidecar.modelUrl || entry.modelUrl || (civitai.modelId ? `https://civitai.com/models/${civitai.modelId}` : '')),
  };
  if (!detailed) return result;
  result.modelDescription = text(sidecar.modelDescription || sidecar.description || civitai.model?.description || entry.description);
  result.versionDescription = text(sidecar.versionDescription || civitai.description);
  result.recommendedSettings = sidecar.recommendedSettings && typeof sidecar.recommendedSettings === 'object'
    ? Object.fromEntries(Object.entries(sidecar.recommendedSettings).filter(([key,value]) => key.length < 100 && ['number','string','boolean'].includes(typeof value)).slice(0, 40)) : null;
  const images = sidecar.images || civitai.images;
  result.images = Array.isArray(images) ? images.slice(0, 4).map(image => ({
    meta: Object.fromEntries(Object.entries(image?.meta || {}).filter(([key]) => ['sampler','steps','cfgScale','clipSkip','seed'].includes(key))),
  })) : [];
  return result;
}
const normalize = value => String(value || '').replace(/\\/g, '/');
function visibleModel(models, name, kind) {
  const matches = models.filter(model => normalize(model.name) === normalize(name) && (kind === 'lora' ? model.kind === 'lora' : model.kind !== 'lora'));
  return matches.length === 1 ? matches[0] : null;
}
module.exports = { catalogDetails, visibleModel };
