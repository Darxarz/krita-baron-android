const express = require('express');
const request = require('supertest');
const { KritaDeviceService, PREFIX, hash, scopeGuard } = require('../services/kritaDeviceService');
const { createKritaIntegration } = require('../routes/kritaIntegration');

describe('Krita device authorization', () => {
  let now, store, issue;
  beforeEach(() => {
    now = 100000;
    issue = jest.fn(async () => PREFIX + 'device-key');
    store = new KritaDeviceService({ now: () => now, issueToken: issue, maxPending: 2 });
  });
  test('approval is explicit; token can be redeemed exactly once', async () => {
    const start = store.start();
    expect(store.poll(start.device_code)).toEqual({ error: 'authorization_pending' });
    await store.approve(start.user_code, 12);
    expect(store.poll(start.device_code)).toEqual({ error: 'slow_down' });
    now += 5000;
    expect(store.poll(start.device_code).access_token).toBe(PREFIX + 'device-key');
    expect(store.poll(start.device_code)).toEqual({ error: 'expired_token' });
    expect(issue).toHaveBeenCalledWith(12);
  });
  test('parallel approvals do not issue duplicate device credentials', async () => {
    const start = store.start();
    const first = store.approve(start.user_code, 12);
    await expect(store.approve(start.user_code, 13)).rejects.toThrow('already used');
    await first;
    expect(issue).toHaveBeenCalledTimes(1);
  });
  test('expired and denied codes never issue tokens', async () => {
    const expired = store.start();
    now += 600001;
    await expect(store.approve(expired.user_code, 12)).rejects.toThrow('expired');
    const denied = store.start();
    await store.approve(denied.user_code, 12, true);
    expect(store.poll(denied.device_code)).toEqual({ error: 'access_denied' });
    expect(issue).not.toHaveBeenCalled();
  });
  test('pending store is bounded and recovers after expiry', () => {
    store.start(); store.start();
    expect(() => store.start()).toThrow('Too many');
    now += 600001;
    expect(store.start().user_code).toMatch(/^[A-F0-9]{10}$/);
  });
});

describe('scoped integration routes', () => {
  let app, keys, now, models;
  const user = { id: 12, username: 'Test artist', coins: 90 };
  const headers = { Authorization: 'Bearer website-session' };
  beforeEach(() => {
    now = 100000;
    keys = new Map();
    models = jest.fn(async () => [{ name: 'Qwen21/model.safetensors', kind: 'checkpoint', title: 'Qwen 2.1', family: 'Qwen21', preview: '' }]);
    app = express(); app.use(express.json()); app.use(scopeGuard);
    app.use(createKritaIntegration({
      now: () => now,
      ApiKey: { create: async (key) => keys.set(key.keyHash, key), update: async (changes, { where }) => Object.assign(keys.get(where.keyHash) || {}, changes) },
      authenticate: async (token) => token === 'website-session' || keys.get(hash(token))?.status === 'active' ? user : null,
      billing: { config: async () => ({ baseCost: 1, rate: 0.0000067 }), compute: (config, graph) => (config.baseCost + Math.ceil(graph['1'].inputs.width * graph['1'].inputs.height * config.rate)) * (graph['1'].inputs.batch_size || 1) },
      getModels: models,
    }));
    app.get('/api/user/info', (req, res) => res.json({ privateAccount: true }));
  });
  async function connectDevice() {
    const start = (await request(app).post('/api/krita/device/start').send({})).body;
    await request(app).post('/api/krita/device/approve').set(headers).send({ user_code: start.user_code }).expect(200);
    const result = await request(app).post('/api/krita/device/poll').send({ device_code: start.device_code }).expect(200);
    return result.body.access_token;
  }
  test('no IP fallback, valid device login, named account and catalog', async () => {
    await request(app).get('/api/krita/account').expect(401);
    await request(app).post('/api/krita/device/approve').send({ user_code: 'ABCDEF0123' }).expect(401);
    const token = await connectDevice();
    expect(token.startsWith(PREFIX)).toBe(true);
    const key = keys.get(hash(token));
    expect(key.type).toBe('krita_device');
    expect(key.expiresAt.getTime()).toBe(now + 30 * 86400000);
    expect(key).not.toHaveProperty('token');
    const account = await request(app).get('/api/krita/account').auth(token, { type: 'bearer' }).expect(200);
    expect(account.body.coins).toBe(90);
    expect(account.body).not.toHaveProperty('passwordHash');
    expect((await request(app).get('/api/krita/models').auth(token, { type: 'bearer' }).expect(200)).body.items[0].family).toBe('Qwen21');
    expect(models).toHaveBeenCalledWith(user);
  });
  test('device cannot access account APIs or authorize another device', async () => {
    const token = await connectDevice();
    await request(app).get('/api/user/info').auth(token, { type: 'bearer' }).expect(403);
    await request(app).get('/api/user/info').set('X-Api-Key', token).expect(403);
    await request(app).get('/api/user/info').query({ token }).expect(403);
    await request(app).get('/krita/' + encodeURIComponent(token).replace('o', '%6F') + '/system_stats').expect(403);
    await request(app).post('/api/krita/device/approve').auth(token, { type: 'bearer' }).send({ user_code: 'ABCDEF0123' }).expect(403);
  });
  test('quote accounts for batch in bleatbucks only, never modifies balance', async () => {
    const token = await connectDevice();
    const quote = await request(app).post('/api/krita/quote').auth(token, { type: 'bearer' }).send({ currency: 'USD', prompt: { '1': { class_type: 'EmptyLatentImage', inputs: { width: 1024, height: 1024, batch_size: 3 } } } }).expect(200);
    expect(quote.body.coins).toBe(27);
    expect(quote.body.unit).toBe('bleatbucks');
    expect(quote.body).not.toHaveProperty('fiat');
    expect(quote.body).not.toHaveProperty('currency');
    expect(quote.body.estimate).toBe(true);
    expect(user.coins).toBe(90);
    await request(app).post('/api/krita/quote').auth(token, { type: 'bearer' }).send({ prompt: [] }).expect(400);
  });
  test('logout revokes device credential without logging out website', async () => {
    const token = await connectDevice();
    await request(app).post('/api/krita/logout').auth(token, { type: 'bearer' }).send({}).expect(200);
    await request(app).get('/api/krita/account').auth(token, { type: 'bearer' }).expect(401);
    await request(app).get('/api/krita/account').set(headers).expect(200);
  });
  test('consent page has CSP, no embedded secret and manual code entry', async () => {
    const result = await request(app).get('/connect/krita').expect(200);
    expect(result.headers['content-security-policy']).toContain("frame-ancestors 'none'");
    expect(result.text).toContain('id="code"');
    expect(result.text).not.toContain('website-session');
    await request(app).get('/connect/krita-assets/connect.js').expect(200);
  });
  test('browser consent offers all plugin languages and bleatbucks terminology', async () => {
    const result = await request(app).get('/connect/krita-assets/translations.json').expect(200);
    expect(Object.keys(result.body)).toHaveLength(14);
    expect(result.body.en.translations['Connected · {name}\nBalance: {amount} bleatbucks']).toContain('bleatbucks');
    expect(result.body.ru.translations['Connected · {name}\nBalance: {amount} bleatbucks']).toContain('беелетиков');
    for (const language of Object.values(result.body)) {
      expect(language.translations['Code from Krita']).toBeTruthy();
      expect(language.translations['Approved. Return to Krita to finish connecting.']).toBeTruthy();
    }
  });
});

