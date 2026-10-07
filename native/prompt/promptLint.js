/**
 * promptLint — domain-aware spell-check engine for image-generation prompts.
 *
 * Prompts are comma-separated booru/SD tags (often multi-word), not natural
 * language, plus weight syntax like (tag:1.2), [tag], <lora:name:0.8>. A normal
 * dictionary flags all of that as wrong. Instead we treat the known tag
 * vocabulary (Danbooru + e621, already loaded by TagManager) as the dictionary:
 *   - exact match  -> recognized tag (coloured by category)
 *   - near match   -> likely typo with a "did you mean" suggestion
 *   - no match     -> custom word (LoRA trigger, niche term) — left alone
 *
 * The engine is pure/UI-free so it can be reused and tested.
 */

export const CATEGORY_COLORS = {
  general: '#378ADD',
  artist: '#D4537E',
  copyright: '#7F77DD',
  character: '#639922',
  meta: '#888780',
  custom: '#9aa0a6',
};

export function categoryColor(category) {
  return CATEGORY_COLORS[category] || CATEGORY_COLORS.general;
}

/** Ниже этой популярности тег не предлагается как исправление опечатки. */
export const SUGGEST_MIN_COUNT = 200;

/** Levenshtein distance with an early-abort ceiling (returns >ceil if it exceeds). */
function boundedLevenshtein(a, b, ceil) {
  const m = a.length;
  const n = b.length;
  if (Math.abs(m - n) > ceil) return ceil + 1;
  if (m === 0) return n;
  if (n === 0) return m;
  let prevRow = new Array(n + 1);
  for (let j = 0; j <= n; j++) prevRow[j] = j;
  for (let i = 1; i <= m; i++) {
    let curr = new Array(n + 1);
    curr[0] = i;
    let rowMin = curr[0];
    const ca = a.charCodeAt(i - 1);
    for (let j = 1; j <= n; j++) {
      const cost = ca === b.charCodeAt(j - 1) ? 0 : 1;
      curr[j] = Math.min(prevRow[j] + 1, curr[j - 1] + 1, prevRow[j - 1] + cost);
      if (curr[j] < rowMin) rowMin = curr[j];
    }
    if (rowMin > ceil) return ceil + 1;
    prevRow = curr;
  }
  return prevRow[n];
}

/**
 * Build a fast lookup index from the loaded tag list.
 * tags: Array<{ tag: string, category?: string, count?: number }>
 * Returns { known: Map<string, category>, byFirst: Map<char, Array<{t,len}>> }.
 */
export function buildIndex(tags) {
  const known = new Map();
  const byFirst = new Map();
  if (!Array.isArray(tags)) return { known, byFirst };
  for (let i = 0; i < tags.length; i++) {
    const entry = tags[i];
    const raw = entry && entry.tag;
    if (!raw) continue;
    const t = String(raw).toLowerCase().replace(/_/g, ' ').trim();
    if (!t) continue;
    if (known.has(t)) continue;
    known.set(t, entry.category || 'general');
    // В корзины для нечёткого поиска кладём только популярные теги: подсказки и
    // так отбрасывают редкие (SUGGEST_MIN_COUNT), а на слабых машинах лишние
    // ~100k объектов — это десятки мегабайт впустую.
    const count = entry.count || 0;
    if (count < SUGGEST_MIN_COUNT) continue;
    // Bucket by the first TWO characters: a typo almost never corrupts the very
    // start of a tag, so this cuts the fuzzy scan enormously while staying correct.
    const key = t.slice(0, 2);
    let bucket = byFirst.get(key);
    if (!bucket) { bucket = []; byFirst.set(key, bucket); }
    bucket.push({ t, len: t.length, count });
  }
  return { known, byFirst };
}

/** Words that differ only by a plural/possessive tail are the same thing, not typos. */
function samePlural(a, b) {
  const [short, long] = a.length <= b.length ? [a, b] : [b, a];
  if (long === `${short}s`) return true;
  if (long === `${short}es`) return true;
  if (short.endsWith('y') && long === `${short.slice(0, -1)}ies`) return true;
  return false;
}

/**
 * True when `word` is just an inflected form of a tag we already know
 * (scarred -> scar, glowing -> glow, muscles -> muscle). Those are correct English,
 * not typos, so they must never be "corrected".
 */
export function isInflectionOfKnown(word, known) {
  if (word.length < 4) return false;
  const stems = [];
  const push = (s) => { if (s && s.length >= 3) stems.push(s); };
  if (word.endsWith('ies')) push(`${word.slice(0, -3)}y`);
  if (word.endsWith('es')) push(word.slice(0, -2));
  if (word.endsWith('s')) push(word.slice(0, -1));
  if (word.endsWith('ing')) { push(word.slice(0, -3)); push(`${word.slice(0, -3)}e`); }
  if (word.endsWith('ed')) { push(word.slice(0, -2)); push(word.slice(0, -1)); }
  if (word.endsWith('er')) { push(word.slice(0, -2)); push(word.slice(0, -1)); }
  if (word.endsWith('ly')) push(word.slice(0, -2));
  // Doubled final consonant before the suffix: scarred -> scar, running -> run
  for (const s of stems.slice()) {
    if (s.length >= 4 && s[s.length - 1] === s[s.length - 2]) push(s.slice(0, -1));
  }
  for (const s of stems) if (known.has(s)) return true;
  return false;
}

/** Is `word` a plausible typo of `cand`? Strict on purpose. */
function isTypoOf(word, cand) {
  if (word === cand) return false;
  if (samePlural(word, cand)) return false;
  if (word.length < 5) return false;
  const ceil = word.length >= 9 ? 2 : 1;
  if (Math.abs(word.length - cand.length) > ceil) return false;
  if (word[0] !== cand[0] || word[1] !== cand[1]) return false;
  const d = boundedLevenshtein(word, cand, ceil);
  return d >= 1 && d <= ceil;
}

/** Normalise a raw segment for lookup: strip weight/bracket syntax, lowercase. */
export function normalizeSegment(core) {
  return core
    .toLowerCase()
    .replace(/^[([{]+/, '')
    .replace(/[)\]}]+$/, '')
    .replace(/:\s*-?\d+(\.\d+)?$/, '')
    .replace(/\\/g, '')
    .trim();
}

/**
 * Common English words that aren't booru tags — left alone so the checker doesn't
 * "correct" ordinary words toward a near tag (e.g. lean -> leaf). Small on purpose;
 * the length/prefix/distance gates below do most of the work, and users can add
 * their own words to the dictionary.
 */
export const COMMON_WORDS = new Set([
  'lean', 'slim', 'slender', 'curvy', 'chubby', 'stocky', 'lanky', 'petite', 'tall', 'short',
  'gentle', 'fierce', 'serene', 'calm', 'moody', 'vibrant', 'muted', 'pale', 'rich', 'deep',
  'glossy', 'matte', 'velvet', 'leather', 'denim', 'cotton', 'satin', 'silk', 'woolen', 'linen',
  'golden', 'silver', 'bronze', 'copper', 'crimson', 'scarlet', 'azure', 'teal', 'amber', 'ivory',
  'beautiful', 'gorgeous', 'elegant', 'graceful', 'majestic', 'ancient', 'modern', 'mystical',
  'magical', 'ethereal', 'radiant', 'luminous', 'cozy', 'warm', 'cool', 'soft', 'sharp', 'smooth',
  'rough', 'shiny', 'misty', 'foggy', 'snowy', 'rainy', 'sunny', 'stormy', 'dreamy', 'gloomy',
  'happy', 'sad', 'angry', 'tired', 'young', 'older', 'small', 'large', 'huge', 'tiny',
  'wearing', 'holding', 'standing', 'sitting', 'lying', 'running', 'looking', 'facing', 'background',
  'foreground', 'detailed', 'realistic', 'simple', 'complex', 'colorful', 'bright', 'dark', 'light',
]);

/**
 * Find the closest known tag for a misspelt phrase, or null.
 *
 * Deliberately strict — a wrong "correction" is far worse than a missed one:
 *  - the phrase must keep its word count (so "hairy belly" can't become "hair bell")
 *  - exactly ONE word may differ, and it must look like a real typo of its counterpart
 *  - plural/singular variants are never "typos"
 *  - the candidate must be a reasonably used tag, so we never propose obscure junk
 */
export function suggestTag(phrase, index) {
  if (phrase.length < 5) return null;
  const bucket = index.byFirst.get(phrase.slice(0, 2));
  if (!bucket) return null;

  const words = phrase.split(/\s+/);
  const MIN_COUNT = SUGGEST_MIN_COUNT;
  let best = null;
  let bestScore = Infinity;

  for (let i = 0; i < bucket.length; i++) {
    const cand = bucket[i];
    if (cand.count < MIN_COUNT) continue;
    if (Math.abs(cand.len - phrase.length) > 2) continue;
    if (cand.t === phrase) return null; // exact match: not a typo at all

    if (words.length === 1) {
      if (!isTypoOf(phrase, cand.t)) continue;
      const score = boundedLevenshtein(phrase, cand.t, 2);
      if (score < bestScore) { bestScore = score; best = cand.t; }
      continue;
    }

    const cw = cand.t.split(/\s+/);
    if (cw.length !== words.length) continue;
    let differing = -1;
    let ok = true;
    for (let w = 0; w < words.length; w++) {
      if (words[w] === cw[w]) continue;
      if (differing !== -1) { ok = false; break; } // more than one word differs
      differing = w;
    }
    if (!ok || differing === -1) continue;
    if (!isTypoOf(words[differing], cw[differing])) continue;
    const score = boundedLevenshtein(words[differing], cw[differing], 2);
    if (score < bestScore) { bestScore = score; best = cand.t; }
  }

  return best ? { tag: best, distance: bestScore } : null;
}

/** Split prompt text into comma-separated segments with char offsets. */
export function splitSegments(text) {
  const segs = [];
  let start = 0;
  for (let i = 0; i <= text.length; i++) {
    if (i === text.length || text[i] === ',') {
      const raw = text.slice(start, i);
      const lead = raw.length - raw.replace(/^\s+/, '').length;
      const core = raw.trim();
      segs.push({ coreStart: start + lead, core });
      start = i + 1;
    }
  }
  return segs;
}

/**
 * Analyse a prompt. Returns { segments, stats }.
 * Each segment: { coreStart, coreEnd, core, norm, status, category?, suggestion? }
 *   status: 'known' | 'typo' | 'unknown' | 'skip'
 * `customSet` is a Set of normalized words the user added to their dictionary.
 * `cache` (optional Map) memoises per-norm results across keystrokes.
 */
export function analyzePrompt(text, index, customSet, cache) {
  const segments = [];
  let known = 0, typo = 0, unknown = 0, total = 0;
  for (const seg of splitSegments(text)) {
    const core = seg.core;
    const coreEnd = seg.coreStart + core.length;
    if (!core) { segments.push({ ...seg, coreEnd, status: 'skip' }); continue; }
    if (/^<.*>$/.test(core)) { segments.push({ ...seg, coreEnd, norm: core, status: 'skip' }); continue; }
    const norm = normalizeSegment(core);
    if (!norm) { segments.push({ ...seg, coreEnd, status: 'skip' }); continue; }

    let res;
    if (cache && cache.has(norm)) {
      res = cache.get(norm);
    } else if (customSet && customSet.has(norm)) {
      res = { status: 'known', category: 'custom' };
    } else if (index.known.has(norm)) {
      res = { status: 'known', category: index.known.get(norm) };
    } else if (COMMON_WORDS.has(norm) || isInflectionOfKnown(norm, index.known)) {
      res = { status: 'ok' }; // ordinary/inflected word, not a tag — leave it alone
    } else {
      const s = suggestTag(norm, index);
      res = s ? { status: 'typo', suggestion: s.tag } : { status: 'unknown' };
    }
    if (cache && !(customSet && customSet.has(norm))) cache.set(norm, res);

    if (res.status === 'known') { known++; total++; }
    else if (res.status === 'typo') { typo++; total++; }
    else if (res.status === 'unknown') { unknown++; total++; }
    segments.push({ ...seg, coreEnd, norm, ...res });
  }
  // Health = recognized tags vs (recognized + likely typos). Unknown/custom words
  // don't count against it — they're allowed, not errors.
  const healthPct = known + typo > 0 ? Math.round((known / (known + typo)) * 100) : 100;
  return { segments, stats: { known, typo, unknown, total, healthPct } };
}

/* ---- Custom user dictionary (localStorage) ---- */
const CUSTOM_KEY = 'promptSpellcheck_customWords';

export function loadCustomWords() {
  try {
    const raw = localStorage.getItem(CUSTOM_KEY);
    return new Set(raw ? JSON.parse(raw) : []);
  } catch {
    return new Set();
  }
}

export function saveCustomWord(word) {
  try {
    const set = loadCustomWords();
    set.add(word);
    localStorage.setItem(CUSTOM_KEY, JSON.stringify(Array.from(set)));
    return set;
  } catch {
    return loadCustomWords();
  }
}
