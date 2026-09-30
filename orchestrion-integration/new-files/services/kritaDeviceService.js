const crypto = require('crypto');

const PREFIX = 'ork_krita_';
const hash = (value) => crypto.createHash('sha256').update(String(value)).digest('hex');

class KritaDeviceService {
  constructor({ now = Date.now, issueToken, maxPending = 500 } = {}) {
    this.now = now;
    this.issueToken = issueToken;
    this.maxPending = maxPending;
    this.pending = new Map();
  }

  prune() {
    for (const [id, entry] of this.pending) {
      if (entry.expiresAt <= this.now()) this.pending.delete(id);
    }
  }

  start() {
    this.prune();
    if (this.pending.size >= this.maxPending) throw Object.assign(new Error('Too many pending sign-ins'), { status: 429 });
    const deviceCode = crypto.randomBytes(32).toString('hex');
    let userCode;
    do {
      userCode = crypto.randomBytes(5).toString('hex').toUpperCase();
    } while ([...this.pending.values()].some((e) => e.userCode === userCode));
    this.pending.set(hash(deviceCode), { userCode, expiresAt: this.now() + 600000, status: 'pending', lastPoll: 0 });
    return { device_code: deviceCode, user_code: userCode, expires_in: 600, interval: 5 };
  }

  async approve(code, userId, deny = false) {
    this.prune();
    const entry = [...this.pending.values()].find((e) => e.userCode === String(code).replace(/-/g, '').toUpperCase());
    if (!entry || entry.status !== 'pending') throw Object.assign(new Error('Code expired or already used'), { status: 400 });
    if (deny) { entry.status = 'denied'; return; }
    entry.status = 'issuing';
    try {
      const issued = await this.issueToken(userId);
      entry.token = issued;
      entry.status = 'approved';
    } catch (error) { entry.status = 'pending'; throw error; }
  }

  poll(deviceCode) {
    this.prune();
    const id = hash(deviceCode);
    const entry = this.pending.get(id);
    if (!entry) return { error: 'expired_token' };
    if (entry.lastPoll && this.now() - entry.lastPoll < 5000) return { error: 'slow_down' };
    entry.lastPoll = this.now();
    if (entry.status === 'denied') { this.pending.delete(id); return { error: 'access_denied' }; }
    if (entry.status !== 'approved') return { error: 'authorization_pending' };
    this.pending.delete(id);
    return { access_token: entry.token, token_type: 'Bearer', expires_in: 30 * 86400, scope: 'krita' };
  }
}

function extractToken(req) {
  const authorization = String(req.headers.authorization || '');
  return authorization.startsWith('Bearer ') ? authorization.slice(7) : String(req.headers['x-api-key'] || req.query?.token || req.query?.api_key || '');
}

function scopeGuard(req, res, next) {
  const token = extractToken(req);
  let decodedPath;
  try { decodedPath = decodeURIComponent(req.path); }
  catch (_) { return res.status(400).json({ error: 'Invalid path' }); }
  const pathToken = /^\/krita\/([^/]+)/.exec(decodedPath)?.[1] || '';
  if ((token.startsWith(PREFIX) || pathToken.startsWith(PREFIX))
      && !req.path.startsWith('/api/krita/') && !req.path.startsWith('/krita/connection/')) {
    return res.status(403).json({ error: 'This device connection only has access to Krita' });
  }
  next();
}

module.exports = { KritaDeviceService, PREFIX, hash, extractToken, scopeGuard };
