// spec platform-targets (web) and spec quality-gates: Browser tests.
const { test, before, after } = require('node:test');
const assert = require('node:assert');
const { serve, launch, waitReady, status, waitSteps } = require('./common');

let srv, browser;
before(async () => {
  srv = await serve();
  browser = await launch();
});
after(async () => {
  await browser.close();
  srv.server.close();
});

test('the web build loads, runs the Prologue and renders', async () => {
  const page = await browser.newPage({ viewport: { width: 960, height: 540 } });
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await page.goto(srv.url);
  await waitReady(page);
  await waitSteps(page, 30);
  const s = await status(page);
  assert.ok(s.chunks > 0, 'world streamed');
  assert.ok(s.columns > 0, 'columns meshed');
  assert.strictEqual(s.touch, false, 'desktop shows no touch controls');
  assert.deepStrictEqual(errors, []);
  await page.close();
});

test('without WebGL2 the page explains what is needed', async () => {
  const page = await browser.newPage();
  await page.addInitScript(() => {
    const orig = HTMLCanvasElement.prototype.getContext;
    HTMLCanvasElement.prototype.getContext = function (type, ...rest) {
      return type === 'webgl2' ? null : orig.call(this, type, ...rest);
    };
  });
  await page.goto(srv.url);
  await page.waitForFunction(() => window.WN_NO_WEBGL2 === true);
  const text = await page.textContent('#status');
  assert.match(text, /needs WebGL2/);
  await page.close();
});

test('a hidden tab does not advance the simulation', async () => {
  const page = await browser.newPage({ viewport: { width: 640, height: 360 } });
  await page.goto(srv.url);
  await waitReady(page);
  await waitSteps(page, 10);
  // Headless Chromium keeps hidden pages running, so the tab switch is
  // simulated through the Page Visibility API the game listens to.
  const setHidden = (hidden) => page.evaluate((h) => {
    Object.defineProperty(document, 'hidden', { configurable: true, get: () => h });
    Object.defineProperty(document, 'visibilityState', { configurable: true, get: () => (h ? 'hidden' : 'visible') });
    document.dispatchEvent(new Event('visibilitychange'));
  }, hidden);
  const before = (await status(page)).step;
  await setHidden(true);
  await new Promise((r) => setTimeout(r, 10000));
  await setHidden(false);
  await waitSteps(page, 1);
  const after = (await status(page)).step;
  // 10 s at 60 Hz would be 600 steps; the clock clamps a long pause to 0.25 s.
  assert.ok(after - before < 60, `advanced ${after - before} steps while hidden`);
  await page.close();
});
