// Runs the REAL shield (extracted from shell/src/fp_shield.cc) in Chromium via
// Playwright and checks: noise lands, is stable per site+seed, differs across
// sites and seeds, stays invisible, and the patched functions look native.
// Usage: node tests/fp-shield/run.mjs   (exit 1 on any failure)
import { readFileSync } from 'node:fs';
import { createServer } from 'node:http';
import { chromium } from 'playwright';

const src = readFileSync(new URL('../../shell/src/fp_shield.cc', import.meta.url), 'utf8');
const m = src.match(/R"FPSHIELD\(([\s\S]*?)\)FPSHIELD"/);
if (!m) { console.error('shield script not found in fp_shield.cc'); process.exit(1); }
const SHIELD = m[1];
const OFF = process.env.FP_OFF === '1';  // red check: run without the shield

const PROBE = `(async () => {
  const sha = async (buf) => Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256', buf))).map(b => b.toString(16).padStart(2, '0')).join('').slice(0, 16);
  const enc = (s) => new TextEncoder().encode(s);
  const c = document.createElement('canvas'); c.width = 240; c.height = 60;
  const x = c.getContext('2d');
  x.textBaseline = 'top'; x.font = '16px Arial'; x.fillStyle = '#f60'; x.fillRect(100, 1, 62, 20);
  x.fillStyle = '#069'; x.fillText('OpenNyx fp 🦞 Cwm fjordbank', 2, 15);
  x.fillStyle = 'rgba(102,204,0,0.7)'; x.fillText('OpenNyx fp 🦞 Cwm fjordbank', 4, 17);
  const dataUrl = await sha(enc(c.toDataURL()));
  const raw = x.getImageData(0, 0, 240, 60).data;
  const imgData = await sha(raw.buffer);
  // ground truth without the shield: draw the same into a fresh canvas and diff pixels
  let gl = null, glRend = null, glPx = null;
  const gc = document.createElement('canvas'); gc.width = 16; gc.height = 16;
  const g = gc.getContext('webgl');
  if (g) {
    const ext = g.getExtension('WEBGL_debug_renderer_info');
    glRend = ext ? g.getParameter(ext.UNMASKED_RENDERER_WEBGL) : null;
    g.clearColor(0.2, 0.4, 0.6, 1); g.clear(g.COLOR_BUFFER_BIT);
    const px = new Uint8Array(16 * 16 * 4); g.readPixels(0, 0, 16, 16, g.RGBA, g.UNSIGNED_BYTE, px);
    glPx = await sha(px.buffer); gl = true;
  }
  let audio = null;
  try {
    const ac = new OfflineAudioContext(1, 5000, 44100);
    const o = ac.createOscillator(); o.type = 'triangle'; o.frequency.value = 10000;
    const comp = ac.createDynamicsCompressor(); o.connect(comp); comp.connect(ac.destination); o.start(0);
    const buf = await ac.startRendering(); const d = buf.getChannelData(0);
    let s = 0; for (let i = 4500; i < 5000; i++) s += Math.abs(d[i]); audio = s.toFixed(12);
  } catch (e) { audio = 'err ' + e.message; }
  const native = [HTMLCanvasElement.prototype.toDataURL, CanvasRenderingContext2D.prototype.getImageData,
    WebGLRenderingContext.prototype.getParameter, Function.prototype.toString]
    .map(f => Function.prototype.toString.call(f)).every(s => /\\{ \\[native code\\] \\}$/.test(s));
  const noProto = HTMLCanvasElement.prototype.toDataURL.prototype === undefined;
  const nameOk = HTMLCanvasElement.prototype.toDataURL.name === 'toDataURL';
  return { dataUrl, imgData, glRend, glPx, audio, hc: navigator.hardwareConcurrency, dm: navigator.deviceMemory ?? null, native, noProto, nameOk };
})()`;

// Two "sites" = two origins on localhost.
const html = '<!doctype html><title>t</title><p>t</p>';
const srv = createServer((q, r) => { r.writeHead(200, { 'content-type': 'text/html' }); r.end(html); });
await new Promise((ok) => srv.listen(0, '127.0.0.1', ok));
const port = srv.address().port;
const SITE_A = `http://127.0.0.1:${port}/`;
const SITE_B = `http://localhost:${port}/`;

const browser = await chromium.launch({ args: ['--use-gl=swiftshader', '--enable-unsafe-swiftshader'] });
async function probe(seed, url) {
  const ctx = await browser.newContext();
  if (!OFF) await ctx.addInitScript(SHIELD.replace('__OPENNYX_SEED__', seed));
  const page = await ctx.newPage();
  await page.goto(url);
  const r = await page.evaluate(PROBE);
  await ctx.close();
  return r;
}
// Visibility check: shield must change at most a few % of pixels by 1 step.
async function pixelDiff(seed, url) {
  const ctx = await browser.newContext();
  if (!OFF) await ctx.addInitScript(SHIELD.replace('__OPENNYX_SEED__', seed));
  const page = await ctx.newPage(); await page.goto(url);
  const res = await page.evaluate(() => {
    const draw = () => { const c = document.createElement('canvas'); c.width = 200; c.height = 50; const x = c.getContext('2d'); x.fillStyle = '#123'; x.fillRect(0, 0, 200, 50); x.fillStyle = '#fc0'; x.font = '20px Arial'; x.fillText('Hallo Bär', 10, 30); return c; };
    const c = draw();
    // read the TRUE pixels via drawImage into WebGL-free path: putImageData round trip is noised,
    // so compare noised getImageData against a decoded toDataURL of the same canvas instead.
    const a = c.getContext('2d').getImageData(0, 0, 200, 50).data;
    return new Promise((ok) => { const img = new Image(); img.onload = () => {
      const t = document.createElement('canvas'); t.width = 200; t.height = 50; const tx = t.getContext('2d'); tx.drawImage(img, 0, 0);
      // both are noised independently; count pixels differing by more than 2 per channel
      const b = CanvasRenderingContext2D.prototype.getImageData.call(tx, 0, 0, 200, 50).data;
      let big = 0, changed = 0; for (let i = 0; i < a.length; i++) { const d = Math.abs(a[i] - b[i]); if (d) changed++; if (d > 2) big++; }
      ok({ big, changed, total: a.length });
    }; img.src = c.toDataURL(); });
  });
  await ctx.close(); return res;
}

const S1 = 'a'.repeat(32), S2 = 'b'.repeat(32);
const a1 = await probe(S1, SITE_A), a1b = await probe(S1, SITE_A);
const b1 = await probe(S1, SITE_B), a2 = await probe(S2, SITE_A);
const vis = await pixelDiff(S1, SITE_A);
await browser.close(); srv.close();

let fail = 0, skipped = 0;
const check = (name, ok, detail = '') => { console.log(`${ok ? '✅' : '❌'} ${name}${detail ? '  ' + detail : ''}`); if (!ok) fail++; };
const diff = (k) => a1[k] !== b1[k] && a1[k] !== a2[k];
check('canvas toDataURL stable per site+seed', a1.dataUrl === a1b.dataUrl, a1.dataUrl);
check('canvas toDataURL differs across sites and seeds', diff('dataUrl'), `${a1.dataUrl} / ${b1.dataUrl} / ${a2.dataUrl}`);
check('canvas getImageData differs across sites and seeds', diff('imgData'));
if (a1.glPx === null) {
  // No GPU / no SwiftShader here: say so instead of reporting green.
  console.log('⚪ WebGL not available in this Chromium, WebGL checks NOT run (run on a machine with a GPU)');
  skipped++;
} else {
  check('WebGL renderer string is the common value', /Intel\(R\) UHD Graphics/.test(a1.glRend || ''), a1.glRend);
  check('WebGL readPixels differs across sites and seeds', diff('glPx'));
}
check('audio fingerprint stable per site+seed', a1.audio === a1b.audio, a1.audio);
check('audio fingerprint differs across sites and seeds', diff('audio'), `${a1.audio} / ${b1.audio} / ${a2.audio}`);
check('hardwareConcurrency = 8', a1.hc === 8, String(a1.hc));
check('patched functions look native', a1.native && a1.noProto && a1.nameOk);
check('noise is invisible (no channel off by more than 2)', vis.big === 0, `${vis.changed}/${vis.total} channel values touched`);
console.log(fail ? `\n${fail} failed` : `\nall ok${skipped ? ' (' + skipped + ' group skipped, see ⚪)' : ''}`);
process.exit(fail ? 1 : 0);
