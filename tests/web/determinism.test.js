// spec platform-targets: Cross-platform determinism - the web build runs the
// same game-test script as the native build and must reach the same checksums.
const { test, before, after } = require('node:test');
const assert = require('node:assert');
const { serve, launch } = require('./common');

let srv, browser;
before(async () => {
  srv = await serve();
  browser = await launch();
});
after(async () => {
  await browser.close();
  srv.server.close();
});

test('the determinism script passes in the browser', async () => {
  const page = await browser.newPage({ viewport: { width: 640, height: 360 } });
  await page.goto(srv.url + '?test=determinism.wntest');
  await page.waitForFunction(() => !!window.WN_TEST_RESULT, null, { timeout: 600000 });
  const result = await page.evaluate(() => window.WN_TEST_RESULT);
  assert.match(result, /^GAME TEST PASSED/, result);
  await page.close();
});
