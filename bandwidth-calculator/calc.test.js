// Unit tests for the calculation block in bandwidth_calculator.html.
// Run from the repo root: node --test bandwidth-calculator/calc.test.js
'use strict';
const test = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const html = fs.readFileSync(path.join(__dirname, 'bandwidth_calculator.html'), 'utf8');
const match = html.match(/<script id="calc">\r?\n([\s\S]*?)<\/script>/);
assert.ok(match, 'calc script block not found');
const ctx = {};
vm.createContext(ctx);
vm.runInContext(match[1], ctx);
const C = ctx.SiPMCalc;

function near(actual, expected, relTol, msg) {
  assert.ok(Math.abs(actual - expected) <= Math.abs(expected) * relTol,
    `${msg || ''} expected ${expected} ±${relTol * 100}%, got ${actual}`);
}

const base = {
  mode: '2p', wavelengthNm: 920, na: 0.45, measuredFwhmUm: undefined,
  fovMm: 1.05, scanner: 'resonant', freqKHz: 12, dutyPct: 100,
};

test('2P FWHM at 920 nm, NA 0.45 is 0.770 um (Zipfel, Table 2 of [1])', () => {
  near(C.fwhm('2p', 920, 0.45), 0.770, 0.005);
});

test('1P, confocal and 3P resolution formulas', () => {
  near(C.fwhm('1p', 920, 0.45), 0.51 * 0.92 / 0.45, 1e-9);
  near(C.fwhm('1p-confocal', 920, 0.45), 0.37 * 0.92 / 0.45, 1e-9);
  near(C.fwhm('3p', 920, 0.45) / C.fwhm('2p', 920, 0.45), Math.sqrt(2 / 3), 1e-9);
  near(C.fwhm('3p', 920, 1.0) / C.fwhm('2p', 920, 1.0), Math.sqrt(2 / 3), 1e-9);
});

test('Table 2 of [1]: 10x 0.45 NA, 1.050 mm, 12 kHz resonant -> 16.4 MHz', () => {
  const r = C.compute(base);
  near(r.f3dbMHz, 16.4, 0.03);
  near(r.fwhmUm, 0.770, 0.005);
  assert.strictEqual(r.measured, false);
});

test('Table 2 of [1]: measured 0.99 um -> 12.7 MHz', () => {
  const r = C.compute({ ...base, measuredFwhmUm: 0.99 });
  near(r.f3dbMHz, 12.7, 0.01);
  assert.strictEqual(r.fwhmUm, 0.99);
  assert.strictEqual(r.measured, true);
});

test('Table 2 of [1]: 16x 0.80 NA, 0.655 mm -> 17.9 MHz', () => {
  // Zipfel gives 0.431 um here; the paper rounds to 0.44 um, so expect ~+2%.
  near(C.compute({ ...base, na: 0.8, fovMm: 0.655 }).f3dbMHz, 17.9, 0.03);
});

test('20x 0.70 NA, 0.471 mm -> 11.5 MHz', () => {
  // Table 2 of [1] lists 12.8 MHz for this row. That value corresponds to the
  // 16x row's FWHM (0.44 um); with this row's 0.49 um, eq. A3 gives 11.5 MHz,
  // which is consistent with Table 3 of [1] (5.9 mm/s, 11.1 mm^2/s).
  near(C.compute({ ...base, na: 0.7, fovMm: 0.471 }).f3dbMHz, 11.5, 0.03);
});

test('Derived outputs: points, sample rate, max pixels per line', () => {
  const r = C.compute(base);
  near(r.pointsPerLine, 1.05e-3 / 0.7703e-6, 0.001);
  near(r.pointsPerSec, 2 * 1.05e-3 * 12e3 / 0.7703e-6, 0.001);
  near(r.minSampleMHz, 2 * r.f3dbMHz, 1e-12);
  near(r.maxPixelsPerLine, 2 * r.pointsPerLine, 1e-12);
  // [1] App. A.4: 16x objective supports ~2977 pixels per line
  // using the paper's rounded 0.44 um FWHM for that objective
  near(C.compute({ ...base, fovMm: 0.655, measuredFwhmUm: 0.44 }).maxPixelsPerLine, 2977, 0.001);
});

test('Constant velocity: f3dB = (sqrt2 ln2 / pi) * R, duty scales velocity', () => {
  const p = { ...base, scanner: 'constant', fovMm: 1, freqKHz: 10, measuredFwhmUm: 1, dutyPct: 100 };
  const r = C.compute(p);
  near(r.pointsPerSec, 10e6, 1e-9);
  near(r.f3dbMHz, Math.SQRT2 * Math.LN2 / Math.PI * 10, 1e-9);
  near(C.compute({ ...p, dutyPct: 50 }).f3dbMHz, 2 * r.f3dbMHz, 1e-9);
  near(C.compute({ ...p, dutyPct: undefined }).f3dbMHz, r.f3dbMHz, 1e-9);
});

test('Resonant mode ignores duty cycle, even invalid', () => {
  const ref = C.compute(base).f3dbMHz;
  assert.strictEqual(C.compute({ ...base, dutyPct: undefined }).f3dbMHz, ref);
  assert.strictEqual(C.compute({ ...base, dutyPct: NaN }).f3dbMHz, ref);
  assert.strictEqual(C.compute({ ...base, dutyPct: 0 }).f3dbMHz, ref);
});

test('Nearest filter on a log scale, with table edges', () => {
  assert.strictEqual(C.FILTERS.length, 8);
  assert.strictEqual(C.nearestFilter(16.4).f, 15);
  assert.strictEqual(C.nearestFilter(8).f, 8);
  assert.strictEqual(C.nearestFilter(50).f, 50);
  assert.strictEqual(C.nearestFilter(23).f, 20);   // ln(23/20)=0.140 < ln(27/23)=0.160
  assert.strictEqual(C.nearestFilter(7.99), null);
  assert.strictEqual(C.nearestFilter(50.01), null);
  assert.strictEqual(C.nearestFilter(NaN), null);
  assert.strictEqual(C.compute(base).filter.f, 15);
  assert.deepStrictEqual(
    { ...C.FILTERS[1] }, { f: 10, c1: '330 pF', l: '1500 nH', c2: '330 pF' });
});

test('Invalid or partial input returns null, never NaN', () => {
  const bad = [
    { na: 0 }, { na: 2 }, { na: NaN }, { fovMm: -1 }, { fovMm: undefined },
    { wavelengthNm: 0 }, { freqKHz: 0 }, { mode: 'xx' }, { scanner: 'xx' },
    { measuredFwhmUm: 0 }, { measuredFwhmUm: NaN },
    { scanner: 'constant', dutyPct: 0 }, { scanner: 'constant', dutyPct: 101 },
    { scanner: 'constant', dutyPct: NaN },
  ];
  for (const b of bad) {
    assert.strictEqual(C.compute({ ...base, ...b }), null, JSON.stringify(b));
  }
  const r = C.compute(base);
  for (const k of ['fwhmUm', 'pointsPerLine', 'pointsPerSec', 'f3dbMHz', 'minSampleMHz', 'maxPixelsPerLine']) {
    assert.ok(Number.isFinite(r[k]), k);
  }
});
