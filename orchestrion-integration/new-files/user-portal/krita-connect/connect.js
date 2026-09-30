const code = document.getElementById('code');
const requestedCode = new URLSearchParams(location.search).get('code') || '';
if (/^[a-f0-9]{10}$/i.test(requestedCode)) code.value = requestedCode.toUpperCase();
const status = document.getElementById('status');
const approve = document.getElementById('approve');
const deny = document.getElementById('deny');
let token = '';
let busy = false;
let complete = false;
let account = null;
let translations = {};
let language = 'en';
const tr = (key, values = {}) => (translations[language]?.translations[key] || key).replace(/\{(\w+)\}/g, (_, name) => String(values[name] ?? `{${name}}`));
function renderStatus() {
  status.textContent = account
    ? tr('Connected · {name}\nBalance: {amount} bleatbucks', { name: account.name, amount: account.coins })
    : tr('Sign in on the website through your browser. Krita does not store your password.');
}
async function initLanguage() {
  try {
    const response = await fetch('/connect/krita-assets/translations.json');
    if (!response.ok) throw new Error('Translations unavailable');
    translations = await response.json();
  } catch (_) { translations = { en: { name: 'English', translations: {} } }; }
  const requested = new URLSearchParams(location.search).get('lang') || navigator.language.toLowerCase();
  language = translations[requested] ? requested : requested.startsWith('zh')
    ? (/tw|hk|hant/.test(requested) ? 'zh-tw' : 'zh-cn') : (translations[requested.split('-')[0]] ? requested.split('-')[0] : 'en');
  const select = document.getElementById('language');
  for (const [id, data] of Object.entries(translations)) {
    const option = document.createElement('option'); option.value = id; option.textContent = data.name; select.appendChild(option);
  }
  function render() {
    document.documentElement.lang = language;
    document.title = tr('Connect to Orchestrion') + ' · Krita';
    document.querySelectorAll('[data-key]').forEach((element) => { element.textContent = tr(element.dataset.key); });
    if (complete) status.textContent = tr(complete === 'deny' ? 'Connection denied.' : 'Approved. Return to Krita to finish connecting.');
    else renderStatus();
  }
  select.value = language;
  select.addEventListener('change', () => { language = select.value; render(); });
  render(); updateLogin();
}
async function updateLogin() {
  if (busy || complete) return;
  try {
    const candidate = localStorage.getItem('apiKey') || '';
    if (candidate && candidate !== token) {
      const response = await fetch('/api/krita/account', { headers: { Authorization: `Bearer ${candidate}` } });
      if (response.ok) { account = await response.json(); token = candidate; renderStatus(); }
      else { token = ''; account = null; renderStatus(); }
    } else if (!candidate) { token = ''; account = null; renderStatus(); }
  } catch (_) { token = ''; account = null; status.textContent = tr('Allow website storage and try again.'); }
  const enabled = Boolean(token) && /^[a-f0-9]{10}$/i.test(code.value);
  approve.disabled = !enabled; deny.disabled = !enabled;
  document.getElementById('login').hidden = Boolean(token);
}
async function confirm(reject) {
  if (busy || !token) return;
  busy = true; approve.disabled = true; deny.disabled = true;
  try {
    const response = await fetch('/api/krita/device/approve', { method: 'POST', headers: { Authorization: `Bearer ${token}`, 'Content-Type': 'application/json' }, body: JSON.stringify({ user_code: code.value.toUpperCase(), deny: reject }) });
    const result = await response.json();
    if (!response.ok) throw new Error(result.error);
    complete = reject ? 'deny' : 'approve'; status.textContent = tr(reject ? 'Connection denied.' : 'Approved. Return to Krita to finish connecting.');
  } catch (_) { status.textContent = tr('Connection denied or code expired. Start sign-in again.'); }
  finally { busy = false; if (!complete) { approve.disabled = false; deny.disabled = false; } }
}
code.addEventListener('input', updateLogin);
approve.addEventListener('click', () => confirm(false));
deny.addEventListener('click', () => confirm(true));
window.addEventListener('storage', updateLogin);
window.addEventListener('focus', updateLogin);
setInterval(updateLogin, 3000); initLanguage();
