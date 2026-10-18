// Copyright (c) 2026 Open Cosmic Software. MIT licensed.

#include "fp_shield.h"

#include <random>

const char kFingerprintSeedSwitch[] = "opennyx-fp-seed";

std::string NewFingerprintSeed() {
  std::random_device rd;
  static const char kHex[] = "0123456789abcdef";
  std::string out;
  for (int i = 0; i < 32; ++i) out.push_back(kHex[rd() & 0xF]);
  return out;
}

namespace {

// The shield. tests/fp-shield/ extracts THIS raw string and runs it in real
// Chromium, so there is one source of truth. Keep the delimiters intact.
const char kShieldJs[] = R"FPSHIELD(
(function () {
  'use strict';
  if (window.__opennyxFp) return;
  Object.defineProperty(window, '__opennyxFp', { value: true });

  const SEED = '__OPENNYX_SEED__';
  // Site key: the origin (about:blank / srcdoc iframes inherit their parent's
  // origin, so a fresh iframe does not get fresh, un-noised prototypes).
  let site = '';
  try { site = self.origin || location.origin || ''; } catch (e) {}
  if (!site || site === 'null') { try { site = location.href; } catch (e) {} }

  function hash32(str) {
    let h = 0x811c9dc5;
    for (let i = 0; i < str.length; i++) {
      h ^= str.charCodeAt(i);
      h = Math.imul(h, 0x01000193);
    }
    return h >>> 0;
  }
  const KEY = hash32(SEED + '|' + site);
  function rng(salt) {
    let a = (KEY ^ hash32(String(salt))) >>> 0;
    return function () {
      a = (a + 0x6d2b79f5) >>> 0;
      let t = a;
      t = Math.imul(t ^ (t >>> 15), t | 1);
      t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }

  // Patched functions report themselves as native.
  const nativeNames = new WeakMap();
  const origToString = Function.prototype.toString;
  function fakeToString() {
    if (nativeNames.has(this)) {
      return 'function ' + nativeNames.get(this) + '() { [native code] }';
    }
    return origToString.call(this);
  }
  nativeNames.set(fakeToString, 'toString');
  Function.prototype.toString = fakeToString;

  function patch(proto, name, makeImpl) {
    if (!proto) return;
    const desc = Object.getOwnPropertyDescriptor(proto, name);
    if (!desc || typeof desc.value !== 'function') return;
    const orig = desc.value;
    const impl = makeImpl(orig);
    // Method syntax: no prototype, not constructible, like real methods.
    const wrapped = { [name](...args) { return impl.apply(this, args); } }[name];
    nativeNames.set(wrapped, name);
    Object.defineProperty(proto, name, Object.assign({}, desc, { value: wrapped }));
  }
  function patchGetter(proto, name, value) {
    if (!proto) return;
    const desc = Object.getOwnPropertyDescriptor(proto, name);
    if (!desc || !desc.get) return;
    const get = { get [name]() { return value; } };
    const g = Object.getOwnPropertyDescriptor(get, name).get;
    nativeNames.set(g, 'get ' + name);
    Object.defineProperty(proto, name, Object.assign({}, desc, { get: g }));
  }

  // ---- Canvas -----------------------------------------------------------
  // Flip the lowest bit of ~3% of RGB channels, deterministic per site and
  // position. Invisible, but it changes every hash.
  const MAX_PIXELS = 4096 * 4096;
  function noisePixels(data, w, h) {
    if (!data || !data.length || w * h > MAX_PIXELS) return;
    const r = rng('canvas:' + w + 'x' + h);
    for (let i = 0; i < data.length; i += 4) {
      if (data[i + 3] === 0) { r(); continue; }  // keep transparent pixels
      const v = r();
      if (v < 0.03) {
        const c = i + ((v * 1000) | 0) % 3;
        data[c] = data[c] ^ 1;
      }
    }
  }

  const C2D = self.CanvasRenderingContext2D && CanvasRenderingContext2D.prototype;
  const origGetImageData = C2D && C2D.getImageData;
  const HCanvas = self.HTMLCanvasElement && HTMLCanvasElement.prototype;
  const origToDataURL = HCanvas && HCanvas.toDataURL;
  const origToBlob = HCanvas && HCanvas.toBlob;

  // Copy |canvas| (2d or WebGL) into a noised twin and return the twin.
  function noisedCopy(canvas) {
    const w = canvas.width, h = canvas.height;
    if (!w || !h || w * h > MAX_PIXELS) return null;
    const twin = document.createElement('canvas');
    twin.width = w; twin.height = h;
    const ctx = twin.getContext('2d');
    if (!ctx) return null;
    ctx.drawImage(canvas, 0, 0);
    const img = origGetImageData.call(ctx, 0, 0, w, h);
    noisePixels(img.data, w, h);
    ctx.putImageData(img, 0, 0);
    return twin;
  }

  patch(HCanvas, 'toDataURL', (orig) => function (...args) {
    let twin = null;
    try { twin = noisedCopy(this); } catch (e) {}
    return orig.apply(twin || this, args);
  });
  patch(HCanvas, 'toBlob', (orig) => function (...args) {
    let twin = null;
    try { twin = noisedCopy(this); } catch (e) {}
    return orig.apply(twin || this, args);
  });
  patch(C2D, 'getImageData', (orig) => function (...args) {
    const img = orig.apply(this, args);
    try { noisePixels(img.data, img.width, img.height); } catch (e) {}
    return img;
  });
  if (self.OffscreenCanvasRenderingContext2D) {
    patch(OffscreenCanvasRenderingContext2D.prototype, 'getImageData',
      (orig) => function (...args) {
        const img = orig.apply(this, args);
        try { noisePixels(img.data, img.width, img.height); } catch (e) {}
        return img;
      });
  }

  // ---- WebGL ------------------------------------------------------------
  // A very common Windows Chrome value instead of the real GPU string.
  const GL_VENDOR = 'Google Inc. (Intel)';
  const GL_RENDERER =
    'ANGLE (Intel, Intel(R) UHD Graphics Direct3D11 vs_5_0 ps_5_0, D3D11)';
  for (const G of [self.WebGLRenderingContext, self.WebGL2RenderingContext]) {
    if (!G) continue;
    patch(G.prototype, 'getParameter', (orig) => function (p) {
      if (p === 0x9245) return GL_VENDOR;     // UNMASKED_VENDOR_WEBGL
      if (p === 0x9246) return GL_RENDERER;   // UNMASKED_RENDERER_WEBGL
      return orig.call(this, p);
    });
    patch(G.prototype, 'readPixels', (orig) => function (...args) {
      const res = orig.apply(this, args);
      try {
        const px = args[6];
        if (px && px.length && args[2] && args[3]) noisePixels(px, args[2], args[3]);
      } catch (e) {}
      return res;
    });
  }

  // ---- Audio ------------------------------------------------------------
  // ~1e-7 is far below anything you can hear, but changes every sum/hash.
  const noisedBuffers = new WeakSet();
  function noiseFloats(arr, salt) {
    const r = rng('audio:' + salt + ':' + arr.length);
    for (let i = 0; i < arr.length; i++) arr[i] += (r() - 0.5) * 2e-7;
  }
  if (self.AudioBuffer) {
    patch(AudioBuffer.prototype, 'getChannelData', (orig) => function (ch) {
      const data = orig.call(this, ch);
      try {
        if (!noisedBuffers.has(data)) { noisedBuffers.add(data); noiseFloats(data, ch); }
      } catch (e) {}
      return data;
    });
  }
  if (self.AnalyserNode) {
    patch(AnalyserNode.prototype, 'getFloatFrequencyData', (orig) => function (arr) {
      const res = orig.call(this, arr);
      try { noiseFloats(arr, 'fft'); } catch (e) {}
      return res;
    });
  }

  // ---- Navigator --------------------------------------------------------
  const NavP = self.Navigator && Navigator.prototype;
  patchGetter(NavP, 'hardwareConcurrency', 8);
  patchGetter(NavP, 'deviceMemory', 8);
})();
)FPSHIELD";

}  // namespace

std::string BuildFingerprintShield(const std::string& seed) {
  std::string js(kShieldJs);
  static const std::string kMarker = "__OPENNYX_SEED__";
  const size_t pos = js.find(kMarker);
  if (pos != std::string::npos) js.replace(pos, kMarker.size(), seed);
  return js;
}
