// spec webxr-mode: Entering VR, Stereo rendering in XR, Leaving VR - with an
// emulated Meta Quest 3 (Immersive Web Emulation Runtime).
const { test, before, after } = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { serve, launch, waitReady, status, waitSteps } = require('./common');

const IWER = fs.readFileSync(path.join(__dirname, 'node_modules/iwer/build/iwer.min.js'), 'utf8');
let srv, browser;
before(async () => {
  srv = await serve();
  browser = await launch();
});
after(async () => {
  await browser.close();
  srv.server.close();
});

test('without WebXR no VR button is shown', async () => {
  const page = await browser.newPage();
  await page.goto(srv.url);
  await waitReady(page);
  assert.strictEqual(await page.isVisible('#vr'), false);
  await page.close();
});

test('an emulated headset enters VR, renders two views per frame and returns', async () => {
  const page = await browser.newPage({ viewport: { width: 960, height: 540 } });
  await page.addInitScript(`${IWER};\nwindow.__xrDevice = new IWER.XRDevice(IWER.metaQuest3);\nwindow.__xrDevice.installRuntime({ forceInstall: true });`);
  await page.goto(srv.url);
  await waitReady(page);
  await page.waitForSelector('#vr', { state: 'visible', timeout: 30000 });
  const entered = await page.evaluate(() => window.wnEnterVR());
  assert.strictEqual(entered, true);
  await page.waitForFunction(() => window.wnStatus().xr_frames >= 10, null, { timeout: 120000 });
  const s = await status(page);
  assert.strictEqual(s.xr, true);
  assert.ok(s.xr_views >= 2 * s.xr_frames - 2, `two views per frame (${s.xr_views} views, ${s.xr_frames} frames)`);
  const stepBefore = s.step;
  await page.evaluate(() => window.WN_XR_SESSION.end());
  await page.waitForFunction(() => window.wnStatus().xr === false, null, { timeout: 30000 });
  await waitSteps(page, 10);
  const after = await status(page);
  assert.ok(after.step > stepBefore, 'game continues in flat-screen mode');
  await page.close();
});
