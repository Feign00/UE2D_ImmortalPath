import test from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { fileURLToPath, pathToFileURL } from 'node:url';
import path from 'node:path';

const require = createRequire(import.meta.url);
const { chromium } = require('playwright');
const sharp = require('sharp');

test('offline inspector: load, step, playback, mask, validation, bad file recovery', async () => {
  // Synthetic in-memory fixture, not a modification of project artwork.
  const rgba = Buffer.alloc(16 * 8 * 4);
  for (let frame = 0; frame < 8; frame++) {
    const offset = ((Math.floor(frame / 4) * 4 + 1) * 16 + frame % 4 * 4 + 1) * 4;
    rgba[offset] = 255; rgba[offset + 1] = frame * 24; rgba[offset + 3] = 100;
  }
  const png = await sharp(rgba, { raw: { width: 16, height: 8, channels: 4 } }).png().toBuffer();
  const browser = await chromium.launch({ headless: true,
    ...(process.env.ATLAS_BROWSER_PATH ? { executablePath: process.env.ATLAS_BROWSER_PATH } : {}) });
  try {
    const page = await browser.newPage({ viewport: { width: 1200, height: 1000 } });
    const errors = [], network = [];
    page.on('pageerror', e => errors.push(e.message));
    page.on('request', req => { if (/^https?:/.test(req.url())) network.push(req.url()); });
    await page.clock.install();
    await page.goto(pathToFileURL(path.join(path.dirname(fileURLToPath(import.meta.url)), 'preview_sprite_atlas.html')).href);
    assert.equal(await page.locator('#play').isDisabled(), true);
    await page.locator('#ground').fill('3');
    await page.locator('#file').setInputFiles({ name: 'synthetic-atlas.png', mimeType: 'image/png', buffer: png });
    await page.waitForFunction(() => !document.getElementById('play').disabled);
    assert.equal(await page.locator('#counter').textContent(), '1 / 8');
    await page.locator('#previous').click();
    assert.equal(await page.locator('#counter').textContent(), '8 / 8');
    await page.locator('#next').click();
    assert.equal(await page.locator('#counter').textContent(), '1 / 8');
    await page.locator('#play').click();
    await page.clock.runFor(350);
    assert.notEqual(await page.locator('#counter').textContent(), '1 / 8');
    await page.locator('#play').click();
    const paused = await page.locator('#counter').textContent();
    await page.clock.runFor(350);
    assert.equal(await page.locator('#counter').textContent(), paused);
    await page.locator('#frame').fill('0');
    await page.locator('#background').selectOption('dark');
    const pixel = () => page.locator('#preview').evaluate(canvas =>
      [...canvas.getContext('2d').getImageData(239, 25, 1, 1).data]);
    assert.notDeepEqual(await pixel(), [19, 32, 39, 255]);
    await page.locator('#mode').selectOption('masked');
    assert.deepEqual(await pixel(), [19, 32, 39, 255]);
    await page.locator('#threshold').fill('85');
    assert.deepEqual(await pixel(), [255, 0, 0, 255]);
    await page.locator('#mode').selectOption('source');
    assert.notDeepEqual(await pixel(), [255, 0, 0, 255]);
    await page.locator('#columns').fill('3');
    assert.equal(await page.locator('#play').isDisabled(), true);
    assert.match(await page.locator('#status').textContent(), /参数无效/);
    await page.locator('#columns').fill('4');
    assert.equal(await page.locator('#play').isDisabled(), false);
    await page.locator('#onion').check();
    await page.locator('#zoom').selectOption('2');
    assert.equal(await page.locator('#preview').getAttribute('data-frame'), '0');
    await page.locator('#file').setInputFiles({ name: 'broken.png', mimeType: 'image/png', buffer: Buffer.from('not a PNG') });
    await page.waitForFunction(() => document.getElementById('status').textContent.includes('无法读取'));
    assert.equal(await page.locator('#play').isDisabled(), true);
    assert.deepEqual(errors, []);
    assert.deepEqual(network, []);
  } finally { await browser.close(); }
});
