// spec input-controls: Automatic touch controls on mobile, Touch moves the character,
// Keyboard hides touch controls; spec platform-targets: Phone starts on Mobile preset.
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

test('a phone shows touch controls and the virtual stick moves the warrior', async () => {
  const context = await browser.newContext({
    viewport: { width: 823, height: 412 },
    deviceScaleFactor: 1,
    isMobile: true,
    hasTouch: true,
  });
  const page = await context.newPage();
  await page.goto(srv.url);
  await waitReady(page);
  await waitSteps(page, 10);
  let s = await status(page);
  assert.strictEqual(s.touch, true, 'touch controls visible');
  assert.strictEqual(s.preset, 'Mobile');
  const z0 = s.z;
  const cdp = await context.newCDPSession(page);
  const x = 823 * 0.2, y = 412 * 0.75;
  await cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [{ x, y, id: 1 }] });
  for (let i = 1; i <= 6; i++) {
    await cdp.send('Input.dispatchTouchEvent', { type: 'touchMove', touchPoints: [{ x, y: y - i * 10, id: 1 }] });
  }
  await waitSteps(page, 60);
  s = await status(page);
  await cdp.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
  assert.ok(s.z > z0 + 0.3, `warrior moved forward (z ${z0} -> ${s.z})`);
  // Using the keyboard hides the touch controls.
  await page.keyboard.down('KeyW');
  await waitSteps(page, 5);
  await page.keyboard.up('KeyW');
  s = await status(page);
  assert.strictEqual(s.touch, false, 'keyboard hides touch controls');
  await context.close();
});
