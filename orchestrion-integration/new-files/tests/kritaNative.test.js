const express = require('express');
const request = require('supertest');
const { createKritaIntegration } = require('../routes/kritaIntegration');
const { scopeGuard } = require('../services/kritaDeviceService');

describe('native Android workflow preparation', () => {
  let app, compile, context;
  const user = { id: 17, username: 'Artist', coins: 40 };
  beforeEach(() => {
    compile = jest.fn(async () => ({ prompt: { '1': { class_type: 'SaveImage', inputs: {} } }, batch: 2 }));
    context = jest.fn(async () => ({ object_info: { trusted: true }, diffusion_models: { model: { base_model: 'krea2' } } }));
    app = express(); app.use(express.json()); app.use(scopeGuard);
    app.use(createKritaIntegration({ ApiKey: {}, authenticate: async token => token === 'ork_krita_test' ? user : null,
      billing: { config: async () => ({}), compute: () => 12 }, getModels: async () => [], getNativeContext: context, compileNative: compile }));
  });
  test('requires authenticated named device; never prepares anonymously', async () => {
    await request(app).post('/api/krita/native/prepare').send({ mode: 'generate' }).expect(401);
    expect(context).not.toHaveBeenCalled(); expect(compile).not.toHaveBeenCalled();
  });
  test('uses trusted metadata and reports only bleatbucks without running jobs', async () => {
    const input = { mode: 'edit', model: 'model', object_info: { fake: true }, currency: 'USD' };
    const result = await request(app).post('/api/krita/native/prepare').auth('ork_krita_test', { type: 'bearer' }).send(input).expect(200);
    expect(compile).toHaveBeenCalledWith({ request: input, object_info: { trusted: true }, diffusion_models: { model: { base_model: 'krea2' } } });
    expect(result.body).toMatchObject({ coins: 12, unit: 'bleatbucks', estimate: true, balance: 40 });
    expect(result.body).not.toHaveProperty('currency'); expect(result.body).not.toHaveProperty('fiat'); expect(user.coins).toBe(40);
  });
  test('invalid mode is rejected before resource discovery', async () => {
    await request(app).post('/api/krita/native/prepare').auth('ork_krita_test', { type: 'bearer' }).send({ mode: 'execute-shell' }).expect(400);
    expect(context).not.toHaveBeenCalled();
  });
  test('compiler validation error is safe and does not submit', async () => {
    compile.mockRejectedValue(Object.assign(new Error('Model is not installed'), { status: 400 }));
    const result = await request(app).post('/api/krita/native/prepare').auth('ork_krita_test', { type: 'bearer' }).send({ mode: 'generate' }).expect(400);
    expect(result.body.error).toBe('Model is not installed');
  });
});
