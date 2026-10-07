/**
 * promptSegments — разметка промпта на сегменты («пузыри») и операции над ними.
 *
 * Модуль чистый: без React, DOM и импортов, чтобы его можно было гонять в
 * jest из корня репозитория. Строка промпта — единственный источник правды;
 * все операции принимают строку и возвращают новую строку.
 *
 * Что считается одним сегментом:
 *  - всё между разделителями , ; перевод строки, а также . ! ? перед пробелом;
 *  - любая парная скобочная группа целиком: (tag:1.2), ((tag)), [tag],
 *    [from:to:when], [tag:when], [tag::when], [a|b], {a|b} — запятые,
 *    двоеточия и | внутри скобок сегмент не режут;
 *  - <lora:name:0.8> и прочие <тип:...> — всегда отдельный сегмент;
 *  - BREAK и AND (A1111) — отдельные сегменты-ключевые слова.
 * Экранированные \( \) \[ \] считаются обычными символами. Непарная скобка
 * (например, пока человек её печатает) — тоже обычный символ, чтобы весь
 * хвост промпта не слипался в один пузырь; об ошибке скажет lintPrompt.
 */

const OPENERS = { '(': ')', '[': ']', '{': '}' };
const CLOSERS = { ')': '(', ']': '[', '}': '{' };
const KEYWORDS = ['BREAK', 'AND'];
// Разделители всегда: запятая, точка с запятой, переводы строки и их
// полноширинные варианты (китайская/японская раскладка).
const HARD_SEPARATORS = new Set([',', ';', '\n', '\r', '，', '；', '、', '。']);
// Разделители предложений — только перед пробелом или в конце текста, чтобы
// не резать 1.2, v2.1, «...» внутри слова и имена файлов.
const SOFT_SEPARATORS = new Set(['.', '!', '?']);
const ANGLE_TYPES_RE = /^<([A-Za-z][\w-]*):/;
const LORA_TYPES_RE = /^(lora|lyco|lycoris|locon|loha|lokr|hypernet)$/i;
const NUMBER_RE = /^[-+]?(?:\d+(?:\.\d*)?|\.\d+)$/;
const WORD_CHAR_RE = /[\p{L}\p{N}_]/u;

export const WEIGHT_MIN = 0;
export const WEIGHT_MAX = 2;
export const LORA_MIN = -2;
export const LORA_MAX = 2;
export const WEIGHT_STEP = 0.1;

const isSpace = (ch) => ch === ' ' || ch === '\t' || ch === ' ' || ch === '　';
const isWordChar = (ch) => !!ch && WORD_CHAR_RE.test(ch);

/**
 * Предварительный проход: парные скобки и <...>-группы.
 * match[i] — индекс парной скобки для открывающей в позиции i (или -1);
 * angleEnd[i] — индекс '>' для группы <тип:...>, начинающейся в i (или -1);
 * unmatched — позиции непарных скобок; brokenAngles — позиции «<lora» без '>'.
 */
export function scanBrackets(text) {
  const n = text.length;
  const match = new Int32Array(n).fill(-1);
  const angleEnd = new Int32Array(n).fill(-1);
  const unmatched = [];
  const brokenAngles = [];
  const stack = [];
  for (let i = 0; i < n; i++) {
    const ch = text[i];
    if (ch === '\\') { i++; continue; }
    if (ch === '<') {
      const head = ANGLE_TYPES_RE.exec(text.slice(i, i + 40));
      if (head) {
        let j = i + 1;
        while (j < n && text[j] !== '>' && text[j] !== '<' && text[j] !== '\n') j++;
        if (j < n && text[j] === '>') {
          angleEnd[i] = j;
          i = j;
          continue;
        }
        if (LORA_TYPES_RE.test(head[1])) brokenAngles.push(i);
      } else if (/^<(lora|lyco|lycoris|hypernet)\b/i.test(text.slice(i, i + 12))) {
        brokenAngles.push(i);
      }
      continue;
    }
    if (OPENERS[ch]) { stack.push(i); continue; }
    if (CLOSERS[ch]) {
      const top = stack.length ? stack[stack.length - 1] : -1;
      if (top >= 0 && text[top] === CLOSERS[ch]) {
        stack.pop();
        match[top] = i;
        match[i] = top;
      } else {
        unmatched.push(i);
      }
    }
  }
  for (const i of stack) unmatched.push(i);
  unmatched.sort((a, b) => a - b);
  return { match, angleEnd, unmatched, brokenAngles };
}

function keywordAt(text, i) {
  const prev = i > 0 ? text[i - 1] : '';
  if (isWordChar(prev) || prev === '\\') return null;
  for (const kw of KEYWORDS) {
    if (text.startsWith(kw, i) && !isWordChar(text[i + kw.length] || '')) {
      // «AND» как слово внутри фразы на английском пишут строчными; ключевое
      // слово A1111 — только заглавными и только как отдельное слово.
      return kw;
    }
  }
  return null;
}

/**
 * Разбить промпт на сегменты. Возвращает массив
 * { index, start, end, text, kind, weight, explicit, base, ... },
 * где [start, end) — границы сегмента без окружающих пробелов.
 */
export function segmentPrompt(text) {
  const src = typeof text === 'string' ? text : '';
  const n = src.length;
  const { match, angleEnd } = scanBrackets(src);
  const raw = [];
  let segStart = -1;
  let lastNonSpace = -1;

  const flush = (endExclusive) => {
    if (segStart < 0) return;
    const end = Math.min(endExclusive, lastNonSpace + 1);
    if (end > segStart) raw.push([segStart, end]);
    segStart = -1;
  };
  const begin = (i) => { if (segStart < 0) segStart = i; };

  let i = 0;
  while (i < n) {
    const ch = src[i];
    if (ch === '\\') {
      begin(i);
      lastNonSpace = Math.min(i + 1, n - 1);
      i += 2;
      continue;
    }
    if (OPENERS[ch] && match[i] > i) {
      begin(i);
      lastNonSpace = match[i];
      i = match[i] + 1;
      continue;
    }
    if (ch === '<' && angleEnd[i] > i) {
      // <lora:...> — всегда отдельный пузырь, даже без запятой рядом
      flush(i);
      raw.push([i, angleEnd[i] + 1]);
      i = angleEnd[i] + 1;
      continue;
    }
    if (HARD_SEPARATORS.has(ch)) { flush(i); i++; continue; }
    if (SOFT_SEPARATORS.has(ch)) {
      const next = src[i + 1];
      if (next === undefined || isSpace(next) || next === '\n' || next === '\r') {
        flush(i);
        i++;
        continue;
      }
    }
    if (isSpace(ch)) { i++; continue; }
    if (ch === 'B' || ch === 'A') {
      const kw = keywordAt(src, i);
      if (kw) {
        flush(i);
        raw.push([i, i + kw.length]);
        i += kw.length;
        continue;
      }
    }
    begin(i);
    lastNonSpace = i;
    i++;
  }
  flush(n);

  return raw.map(([start, end], index) => ({
    index,
    start,
    end,
    ...classifySegment(src.slice(start, end)),
  }));
}

/** Индекс сегмента, внутри которого (или на границе которого) стоит каретка. */
export function segmentAt(segments, pos) {
  let lo = 0;
  let hi = segments.length - 1;
  while (lo <= hi) {
    const mid = (lo + hi) >> 1;
    const s = segments[mid];
    if (pos < s.start) hi = mid - 1;
    else if (pos > s.end) lo = mid + 1;
    else return mid;
  }
  return -1;
}

/* ─────────────────────────── Разбор одного сегмента ─────────────────────── */

/** Позиция ':' верхнего уровня перед числом в конце строки (для (tag:1.2)). */
function splitExplicitWeight(inner) {
  const { match } = scanBrackets(inner);
  let colon = -1;
  for (let i = 0; i < inner.length; i++) {
    const ch = inner[i];
    if (ch === '\\') { i++; continue; }
    if (OPENERS[ch] && match[i] > i) { i = match[i]; continue; }
    if (ch === '<') {
      const close = inner.indexOf('>', i);
      if (close > i) { i = close; continue; }
    }
    if (ch === ':') colon = i;
  }
  if (colon < 0) return null;
  const num = inner.slice(colon + 1).trim();
  if (!NUMBER_RE.test(num)) return null;
  return { base: inner.slice(0, colon).trim(), weight: Number.parseFloat(num), raw: num };
}

/** Есть ли ':' или '|' на верхнем уровне — признак [from:to:when] и [a|b]. */
function hasTopLevel(inner, chars) {
  const { match } = scanBrackets(inner);
  for (let i = 0; i < inner.length; i++) {
    const ch = inner[i];
    if (ch === '\\') { i++; continue; }
    if (OPENERS[ch] && match[i] > i) { i = match[i]; continue; }
    if (chars.includes(ch)) return true;
  }
  return false;
}

/** Обёрнута ли строка целиком одной парой скобок данного типа. */
function isWrapped(s, opener) {
  if (s.length < 2 || s[0] !== opener) return false;
  const { match } = scanBrackets(s);
  return match[0] === s.length - 1;
}

const round2 = (x) => Math.round(x * 100) / 100;
const round1 = (x) => Math.round(x * 10) / 10;

/** Число веса в том виде, как его пишут в промптах: 1.1, 0.85, 2. */
export function formatWeight(w) {
  const r = round2(w);
  return String(Object.is(r, -0) ? 0 : r);
}

function parseLora(core) {
  const m = /^<([A-Za-z][\w-]*):([^>]*)>$/.exec(core);
  if (!m) return null;
  const type = m[1];
  if (!LORA_TYPES_RE.test(type)) return { type, extra: true };
  const parts = m[2].split(':');
  const name = parts[0];
  const strengthRaw = parts.length > 1 ? parts[1].trim() : '';
  const rest = parts.slice(2);
  const strengthOk = strengthRaw === '' || NUMBER_RE.test(strengthRaw);
  return {
    type,
    name,
    strengthRaw,
    rest,
    weight: strengthRaw !== '' && strengthOk ? Number.parseFloat(strengthRaw) : 1,
    broken: !name.trim() || !strengthOk,
  };
}

/**
 * Классификация текста сегмента.
 * kind: 'keyword' | 'lora' | 'extra' | 'weighted' | 'schedule' | 'group' | 'tag'
 *   weighted — (tag:1.2), ((tag)), [tag]; weight — итоговый вес;
 *   schedule — [from:to:when], [tag:when], [a|b]: вес не задаёт;
 *   group — {…} (dynamic prompts / NovelAI): вес не задаёт.
 * base — текст без оформления веса (для перевода, поиска в словаре, дублей).
 */
export function classifySegment(core) {
  const text = String(core || '');
  const words = text.trim() ? text.trim().split(/\s+/).length : 0;
  const common = { text, words, sentence: words >= 4 };
  if (KEYWORDS.includes(text)) {
    return { ...common, kind: 'keyword', keyword: text, weight: null, explicit: false, base: text, sentence: false };
  }
  if (text[0] === '<' && text[text.length - 1] === '>') {
    const lora = parseLora(text);
    if (lora && !lora.extra) {
      return {
        ...common,
        kind: 'lora',
        weight: lora.weight,
        explicit: lora.strengthRaw !== '',
        base: lora.name,
        lora,
        sentence: false,
      };
    }
    if (lora && lora.extra) {
      return { ...common, kind: 'extra', weight: null, explicit: false, base: text, sentence: false };
    }
  }
  if (isWrapped(text, '(')) {
    const inner = text.slice(1, -1);
    const exp = splitExplicitWeight(inner);
    if (exp) {
      return { ...common, kind: 'weighted', weight: exp.weight, explicit: true, base: exp.base };
    }
    // ((tag)) — неявный акцент 1.1 на каждую пару скобок
    let depth = 1;
    let body = inner;
    while (isWrapped(body.trim(), '(') && !splitExplicitWeight(body.trim().slice(1, -1))) {
      body = body.trim().slice(1, -1);
      depth++;
    }
    return { ...common, kind: 'weighted', weight: round2(1.1 ** depth), explicit: false, nest: depth, base: body.trim() };
  }
  if (isWrapped(text, '[')) {
    const inner = text.slice(1, -1);
    if (hasTopLevel(inner, [':', '|'])) {
      // [from:to:when], [tag:when], [tag::when], [a|b] — одна единица
      return { ...common, kind: 'schedule', weight: null, explicit: false, base: text, sentence: false };
    }
    let depth = 1;
    let body = inner;
    while (isWrapped(body.trim(), '[') && !hasTopLevel(body.trim().slice(1, -1), [':', '|'])) {
      body = body.trim().slice(1, -1);
      depth++;
    }
    return { ...common, kind: 'weighted', weight: round2(1 / 1.1 ** depth), explicit: false, nest: depth, bracket: true, base: body.trim() };
  }
  if (isWrapped(text, '{')) {
    return { ...common, kind: 'group', weight: null, explicit: false, base: text, sentence: false };
  }
  return { ...common, kind: 'tag', weight: null, explicit: false, base: text };
}

/* ─────────────────────────────── Веса ───────────────────────────────────── */

const clamp = (x, lo, hi) => Math.min(hi, Math.max(lo, x));

/** Текущий вес сегмента (1 — если вес не задан). null — у ключевых слов. */
export function getSegmentWeight(core) {
  const info = classifySegment(core);
  if (info.kind === 'keyword' || info.kind === 'extra') return null;
  if (info.kind === 'lora') return info.weight;
  if (info.kind === 'weighted') return info.explicit ? info.weight : round1(info.weight);
  return 1;
}

/**
 * Поставить сегменту вес w. Для <lora:…> меняется сила LoRA. Вес 1 снимает
 * оформление, если без скобок текст останется одним сегментом.
 */
export function setSegmentWeight(core, w) {
  const info = classifySegment(core);
  if (info.kind === 'keyword' || info.kind === 'extra') return null;
  if (info.kind === 'lora') {
    const { type, name, rest } = info.lora;
    const value = formatWeight(clamp(w, LORA_MIN, LORA_MAX));
    return `<${type}:${name}:${value}${rest.length ? `:${rest.join(':')}` : ''}>`;
  }
  const weight = round2(clamp(w, WEIGHT_MIN, WEIGHT_MAX));
  // schedule/group/tag — оборачиваем сегмент целиком
  const base = info.kind === 'weighted' ? info.base : info.text.trim();
  if (Math.abs(weight - 1) < 1e-9) {
    if (segmentPrompt(base).length <= 1 && !/^\s*$/.test(base)) return base;
    return `(${base}:1)`;
  }
  return `(${base}:${formatWeight(weight)})`;
}

/** Ctrl+↑ / Ctrl+↓: изменить вес на delta (обычно ±0.1). */
export function adjustSegmentWeight(core, delta) {
  const current = getSegmentWeight(core);
  if (current === null) return null;
  const info = classifySegment(core);
  const lo = info.kind === 'lora' ? LORA_MIN : WEIGHT_MIN;
  const hi = info.kind === 'lora' ? LORA_MAX : WEIGHT_MAX;
  const next = clamp(round2(current + delta), lo, hi);
  if (next === current && info.kind !== 'weighted') return core;
  return setSegmentWeight(core, next);
}

/* ─────────────────────────────── Проверка ───────────────────────────────── */

/** Ключ для поиска дублей и словаря: без веса, регистра и подчёркиваний. */
export function normalizeKey(core) {
  const info = typeof core === 'string' ? classifySegment(core) : core;
  if (!info || info.kind === 'keyword') return '';
  if (info.kind === 'lora') return `<lora:${String(info.lora.name).trim().toLowerCase()}>`;
  return String(info.base || '')
    .replace(/\\([()[\]{}])/g, '$1')
    .replace(/_/g, ' ')
    .replace(/\s+/g, ' ')
    .trim()
    .toLowerCase();
}

/**
 * Тихая проверка настоящих ошибок. Неизвестные словарю теги НЕ ошибка:
 * это LoRA-триггеры, редкие слова, естественный язык.
 * Возвращает массив списков проблем по индексу сегмента:
 *   { code: 'unbalanced' | 'brokenLora' | 'weightRange' | 'duplicate', ... }
 */
export function lintPrompt(text, segments) {
  const src = typeof text === 'string' ? text : '';
  const segs = segments || segmentPrompt(src);
  const issues = segs.map(() => null);
  const add = (i, issue) => {
    if (i < 0) return;
    if (!issues[i]) issues[i] = [];
    if (!issues[i].some((x) => x.code === issue.code)) issues[i].push(issue);
  };
  const { unmatched, brokenAngles } = scanBrackets(src);
  const locate = (pos) => {
    // Непарный символ всегда внутри какого-то сегмента: скобки не разделители
    for (let i = 0; i < segs.length; i++) {
      if (pos >= segs[i].start && pos < segs[i].end) return i;
    }
    return -1;
  };
  unmatched.forEach((pos) => add(locate(pos), { code: 'unbalanced', char: src[pos], pos }));
  brokenAngles.forEach((pos) => add(locate(pos), { code: 'brokenLora', pos }));

  const seen = new Map();
  segs.forEach((seg, i) => {
    if (seg.kind === 'lora') {
      if (seg.lora.broken) add(i, { code: 'brokenLora' });
      else if (seg.weight < LORA_MIN || seg.weight > LORA_MAX) add(i, { code: 'weightRange', weight: seg.weight });
    } else if (seg.kind === 'weighted' && seg.explicit) {
      if (seg.weight < WEIGHT_MIN || seg.weight > WEIGHT_MAX) add(i, { code: 'weightRange', weight: seg.weight });
    }
    const key = normalizeKey(seg);
    if (!key) return;
    if (seen.has(key)) add(i, { code: 'duplicate', first: seen.get(key) });
    else seen.set(key, i);
  });
  return issues;
}

/* ─────────────────────────── Операции над текстом ───────────────────────── */

export function replaceRange(text, start, end, insert) {
  return text.slice(0, start) + insert + text.slice(end);
}

/** Заменить текст сегмента i. Возвращает { text, start, end } нового сегмента. */
export function replaceSegment(text, segments, i, core) {
  const seg = segments[i];
  return {
    text: replaceRange(text, seg.start, seg.end, core),
    start: seg.start,
    end: seg.start + core.length,
  };
}

/** Удалить сегмент вместе с одним разделителем, не трогая соседей. */
export function deleteSegment(text, segments, i) {
  const seg = segments[i];
  const next = segments[i + 1];
  const prev = segments[i - 1];
  if (next) {
    // Перевод строки после сегмента сохраняем (абзацы промпта), если можно
    // вместо него съесть разделитель перед сегментом.
    const gapAfter = text.slice(seg.end, next.start);
    const gapBefore = prev ? text.slice(prev.end, seg.start) : '';
    if (prev && gapAfter.includes('\n') && !gapBefore.includes('\n')) {
      return { text: replaceRange(text, prev.end, seg.end, ''), caret: prev.end };
    }
    return { text: replaceRange(text, seg.start, next.start, ''), caret: seg.start };
  }
  if (prev) {
    return { text: replaceRange(text, prev.end, seg.end, ''), caret: prev.end };
  }
  const out = replaceRange(text, seg.start, seg.end, '');
  return { text: out.trim() ? out : '', caret: out.trim() ? seg.start : 0 };
}

/** Очистить вставляемый кусок: без висящих запятых и пробелов по краям. */
export function cleanInsert(s) {
  return String(s || '')
    .replace(/^[\s,;，；]+/, '')
    .replace(/[\s,;，；]+$/, '');
}

/** Вставить текст отдельным сегментом сразу после сегмента i. */
export function insertAfterSegment(text, segments, i, insert) {
  const piece = cleanInsert(insert);
  if (!piece) return null;
  if (!segments.length || i < 0) return appendSegment(text, piece);
  const seg = segments[i];
  const joined = `, ${piece}`;
  return {
    text: replaceRange(text, seg.end, seg.end, joined),
    start: seg.end + 2,
    end: seg.end + joined.length,
  };
}

export function duplicateSegment(text, segments, i) {
  return insertAfterSegment(text, segments, i, segments[i].text);
}

/** Добавить сегмент в конец промпта (перенос в негатив и обратно). */
export function appendSegment(text, piece) {
  const src = typeof text === 'string' ? text : '';
  const clean = cleanInsert(piece);
  if (!clean) return { text: src, start: src.length, end: src.length };
  const trimmed = src.replace(/\s+$/, '');
  if (!trimmed) return { text: clean, start: 0, end: clean.length };
  const sep = /[,;，；]$/.test(trimmed) ? ' ' : ', ';
  const out = trimmed + sep + clean;
  return { text: out, start: out.length - clean.length, end: out.length };
}

/**
 * Переставить сегмент from на место to. Разделители остаются на своих
 * местах — меняется только порядок текстов сегментов.
 */
export function moveSegment(text, segments, from, to) {
  const count = segments.length;
  if (from < 0 || from >= count) return null;
  const target = Math.max(0, Math.min(count - 1, to));
  if (target === from) return null;
  const cores = segments.map((s) => s.text);
  const [moved] = cores.splice(from, 1);
  cores.splice(target, 0, moved);
  let out = '';
  let cursor = 0;
  let newStart = 0;
  segments.forEach((seg, idx) => {
    out += text.slice(cursor, seg.start);
    if (idx === target) newStart = out.length;
    out += cores[idx];
    cursor = seg.end;
  });
  out += text.slice(cursor);
  return { text: out, index: target, start: newStart, end: newStart + moved.length };
}

/** Пробелы ↔ подчёркивания (long hair ↔ long_hair); <lora:…> не трогаем. */
export function toggleUnderscores(core) {
  const info = classifySegment(core);
  if (info.kind === 'lora' || info.kind === 'extra' || info.kind === 'keyword') return null;
  if (core.includes('_')) {
    return core.replace(/([\p{L}\p{N}])_+(?=[\p{L}\p{N}])/gu, '$1 ');
  }
  const out = core.replace(/([\p{L}\p{N}]) +(?=[\p{L}\p{N}])/gu, '$1_');
  return out === core ? null : out;
}

/** Экранировать скобки тега из словаря: «name (series)» → «name \(series\)». */
export function escapeTagForPrompt(tag) {
  // Без lookbehind: старые iOS Safari не разбирают (?<!…) и роняют модуль целиком
  return String(tag || '').replace(/\\?([()[\]])/g, '\\$1');
}

/** Подставить новый «базовый» текст, сохранив вес сегмента. */
export function replaceBase(core, newBase) {
  const info = classifySegment(core);
  const base = cleanInsert(newBase);
  if (!base) return null;
  if (info.kind === 'weighted') {
    const w = info.explicit ? info.weight : round1(info.weight);
    if (Math.abs(w - 1) < 1e-9) return base;
    return `(${base}:${formatWeight(w)})`;
  }
  return base;
}

/**
 * Контекст автодополнения: что человек сейчас печатает. Ищем по всему
 * сегменту до каретки (многословные теги — «long ha» → «long hair»), без
 * ведущих скобок. Возвращает null, если подсказывать не нужно.
 */
export function autocompleteContext(text, caret) {
  const src = typeof text === 'string' ? text : '';
  if (caret <= 0 || caret > src.length) return null;
  let start = caret;
  while (start > 0) {
    const ch = src[start - 1];
    if (HARD_SEPARATORS.has(ch) || ch === '(' || ch === '[' || ch === '{' || ch === '|' || ch === '>') break;
    if (SOFT_SEPARATORS.has(ch) && start < src.length && isSpace(src[start])) break;
    start--;
  }
  while (start < caret && isSpace(src[start])) start++;
  const query = src.slice(start, caret);
  if (query.length < 2 || query.length > 48) return null;
  if (/[:<\\)\]]/.test(query)) return null;
  if (query.split(/\s+/).length > 4) return null;
  if (KEYWORDS.includes(query.trim())) return null;
  // Конец заменяемого куска: хвост слова после каретки
  let end = caret;
  while (end < src.length && isWordChar(src[end])) end++;
  const tail = src.slice(end);
  return {
    from: start,
    to: end,
    query: query.replace(/_/g, ' ').toLowerCase(),
    atEnd: /^\s*$/.test(tail),
  };
}
