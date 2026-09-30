const express = require('express');
const path = require('path');
const { KritaDeviceService, PREFIX, hash, extractToken } = require('../services/kritaDeviceService');

function createKritaIntegration({ ApiKey, authenticate, billing, getModels, getNativeContext, compileNative, now = Date.now }) {
  const router = express.Router();
  const devices = new KritaDeviceService({ now, issueToken: async (userId) => {
    if (ApiKey.count && await ApiKey.count({ where: { userId, type: 'krita_device', status: 'active', expiresAt: { [require('sequelize').Op.gt]: new Date(now()) } } }) >= 10) {
      throw Object.assign(new Error('Отзови одно из старых подключений Krita в кабинете. Лимит: 10 устройств.'), { status: 400 });
    }
    const token = PREFIX + require('crypto').randomBytes(32).toString('hex');
    await ApiKey.create({ userId, keyHash: hash(token), keyPrefix: token.slice(0, 12) + '...', name: 'Krita Baron Edition', type: 'krita_device', expiresAt: new Date(now() + 30 * 86400000), status: 'active' });
    return token;
  } });
  const wrap = (action) => async (req, res) => {
    res.set('Cache-Control', 'no-store');
    try { await action(req, res); }
    catch (error) { res.status(error.status || 503).json({ error: error.status ? error.message : 'Orchestrion integration temporarily unavailable' }); }
  };
  const userFor = async (req) => {
    const token = extractToken(req);
    const user = token && await authenticate(token);
    if (!user) throw Object.assign(new Error('Sign in required'), { status: 401 });
    return user;
  };
  router.get('/connect/krita', (req, res) => {
    res.set({ 'Cache-Control': 'no-store', 'Referrer-Policy': 'no-referrer', 'X-Frame-Options': 'DENY', 'Content-Security-Policy': "default-src 'none'; script-src 'self'; style-src 'self'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'" });
    res.sendFile(path.join(__dirname, '../user-portal/krita-connect/index.html'));
  });
  router.use('/connect/krita-assets', express.static(path.join(__dirname, '../user-portal/krita-connect'), { index: false }));
  router.get('/api/krita/info', (req, res) => res.json({ version: 1, name: 'Orchestrion', capabilities: ['browser-login', 'account', 'quote', 'model-gallery', ...(getNativeContext && compileNative ? ['native-workflow'] : [])], proxyPath: '/krita/connection' }));
  router.post('/api/krita/device/start', wrap(async (req, res) => res.json({ ...devices.start(), verification_uri: '/connect/krita' })));
  router.post('/api/krita/device/poll', wrap(async (req, res) => {
    if (!/^[a-f0-9]{64}$/.test(req.body?.device_code || '')) return res.status(400).json({ error: 'invalid_request' });
    res.json(devices.poll(req.body.device_code));
  }));
  router.post('/api/krita/device/approve', wrap(async (req, res) => {
    if (extractToken(req).startsWith(PREFIX)) return res.status(403).json({ error: 'Use your website login to approve a new device' });
    const user = await userFor(req);
    if (!user.username) return res.status(403).json({ error: 'Sign in to a named account first' });
    if (!/^[A-Fa-f0-9]{10}$/.test(req.body?.user_code || '')) return res.status(400).json({ error: 'Invalid code' });
    await devices.approve(req.body.user_code, user.id, req.body.deny === true);
    res.json({ ok: true });
  }));
  router.get('/api/krita/account', wrap(async (req, res) => {
    const user = await userFor(req);
    res.json({ id: String(user.id), name: user.username || 'Orchestrion', coins: Number(user.coins), billing: await billing.config() });
  }));
  router.post('/api/krita/quote', wrap(async (req, res) => {
    const user = await userFor(req);
    const prompt = req.body?.prompt;
    if (!prompt || typeof prompt !== 'object' || Array.isArray(prompt) || Object.keys(prompt).length > 2000) return res.status(400).json({ error: 'Invalid workflow' });
    const coins = billing.compute(await billing.config(), prompt);
    res.json({ coins, balance: Number(user.coins), unit: 'bleatbucks', estimate: true, settledOnSuccess: true });
  }));
  router.get('/api/krita/models', wrap(async (req, res) => {
    const user = await userFor(req);
    res.json({ items: await getModels(user) });
  }));
  router.post('/api/krita/native/prepare', wrap(async (req, res) => {
    const user = await userFor(req);
    if (!getNativeContext || !compileNative) return res.status(503).json({ error: 'Native workflow compiler is not configured' });
    const input = req.body;
    if (!input || typeof input !== 'object' || Array.isArray(input)
        || !['generate', 'edit', 'upscale', 'background'].includes(input.mode)) return res.status(400).json({ error: 'Invalid workspace' });
    // Server selection, installed resources, and model metadata are always
    // trusted server data. Client-supplied object_info or graphs are ignored.
    const result = await compileNative({ request: input, ...await getNativeContext(user) });
    const coins = billing.compute(await billing.config(), result.prompt);
    res.json({ ...result, coins, balance: Number(user.coins), unit: 'bleatbucks', estimate: true });
  }));
  router.post('/api/krita/logout', wrap(async (req, res) => {
    await userFor(req);
    const token = extractToken(req);
    if (token.startsWith(PREFIX)) await ApiKey.update({ status: 'revoked' }, { where: { keyHash: hash(token), type: 'krita_device' } });
    res.json({ ok: true });
  }));
  return router;
}

module.exports = { createKritaIntegration };
