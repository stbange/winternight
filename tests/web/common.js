// Shared helpers for the browser tests: a static server for the web build
// and a Chromium launcher that works without a GPU (SwiftShader WebGL2).
const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const { chromium } = require('playwright');

const BUILD_DIR = process.env.WN_WEB_BUILD || path.resolve(__dirname, '../../build/web');
const TYPES = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm', '.data': 'application/octet-stream' };

function serve() {
  return new Promise((resolve) => {
    const server = http.createServer((req, res) => {
      const url = decodeURIComponent(req.url.split('?')[0]);
      const file = path.join(BUILD_DIR, url === '/' ? 'winternight.html' : url);
      if (!file.startsWith(BUILD_DIR) || !fs.existsSync(file)) {
        res.writeHead(404);
        res.end();
        return;
      }
      res.writeHead(200, { 'Content-Type': TYPES[path.extname(file)] || 'application/octet-stream' });
      fs.createReadStream(file).pipe(res);
    });
    server.listen(0, '127.0.0.1', () => resolve({ server, url: `http://127.0.0.1:${server.address().port}/winternight.html` }));
  });
}

async function launch() {
  return chromium.launch({
    args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'],
  });
}

async function waitReady(page) {
  await page.waitForFunction(() => window.WN_READY === true, null, { timeout: 180000 });
}

async function status(page) {
  return page.evaluate(() => window.wnStatus());
}

async function waitSteps(page, n) {
  const start = (await status(page)).step;
  await page.waitForFunction((s) => window.wnStatus().step >= s, start + n, { timeout: 120000 });
}

module.exports = { serve, launch, waitReady, status, waitSteps, BUILD_DIR };
