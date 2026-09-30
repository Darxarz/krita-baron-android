// SPDX-License-Identifier: GPL-3.0-or-later
const { spawn } = require('child_process');
const path = require('path');

function createCompiler({ python = process.env.BARON_NATIVE_PYTHON,
  script = process.env.BARON_NATIVE_COMPILER, timeout = 45000, concurrency = 2 } = {}) {
  let active = 0;
  return async (payload) => {
    if (!python || !script || !path.isAbsolute(script)) {
      throw Object.assign(new Error('Native workflow compiler is not configured'), { status: 503 });
    }
    if (active >= concurrency) throw Object.assign(new Error('Workflow compiler is busy. Try again shortly.'), { status: 429 });
    active++;
    try {
      return await new Promise((resolve, reject) => {
        const child = spawn(python, [script], { shell: false, windowsHide: true, env: { ...process.env, QT_QPA_PLATFORM: 'offscreen' } });
        const chunks = [];
        let size = 0, finished = false;
        const finish = (error, result) => {
          if (finished) return;
          finished = true; clearTimeout(timer);
          error ? reject(error) : resolve(result);
        };
        const timer = setTimeout(() => { child.kill(); finish(Object.assign(new Error('Workflow preparation timed out'), { status: 503 })); }, timeout);
        child.stdout.on('data', chunk => {
          size += chunk.length;
          if (size > 120 * 1024 * 1024) { child.kill(); finish(Object.assign(new Error('Workflow is too large'), { status: 413 })); }
          else chunks.push(chunk);
        });
        // Diagnostic output can contain prompts and model paths. Do not copy
        // compiler stderr into public responses or production logs.
        child.stderr.resume();
        child.on('error', () => finish(Object.assign(new Error('Native workflow compiler cannot start'), { status: 503 })));
        child.on('close', code => {
          try {
            const data = JSON.parse(Buffer.concat(chunks).toString('utf8'));
            if (code !== 0 || data.error || !data.prompt) return finish(Object.assign(new Error(data.error || 'Invalid compiler response'), { status: 400 }));
            finish(null, data);
          } catch (_) { finish(Object.assign(new Error('Invalid compiler response'), { status: 503 })); }
        });
        child.stdin.on('error', () => {});
        child.stdin.end(JSON.stringify(payload));
      });
    } finally { active--; }
  };
}
module.exports = { createCompiler };
