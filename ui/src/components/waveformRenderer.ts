/**
    Pure drawing helpers for the waveform panel. The bars are drawn into a cached layer
    (they only change with data or selection); liquid lines, particles and the playhead are
    drawn every frame on top.
*/

export const WAVE_W = 792;
export const WAVE_H = 172;
export const PAD_X = 34;
// Vertical layout of the 172 px panel: Reset strip 0-36, waveform band 44-124, button strip 134-162.
export const CENTER_Y = 84;
export const MAX_HALF = 40;
export const INNER_W = WAVE_W - PAD_X * 2;

export interface BarLayerInput {
  view: "raw" | "edit";
  peaks: number[];
  /** EDIT: where the original segment begins, as a 0..1 fraction of the clip. */
  originalFraction: number | null;
  /** RAW: selection as 0..1 fractions of the source. */
  selection: [number, number] | null;
}

const lerp = (a: number, b: number, t: number) => a + (b - a) * t;

/** Colours of the waveform canvases. Strings are "r, g, b" triples; alpha is set where drawn. */
export interface WaveTint {
  baseline: string;
  glow: string;
  /** EDIT bar colour for a 0..1 level; `original` marks bars of the original phrase. */
  bar: (norm: number, original: boolean) => string;
  /** Live-recording bar colour for a 0..1 level. */
  liveBar: (norm: number) => string;
  rawOn: string;
  rawOff: string;
  selection: string;
  handle: string;
  handleGlow: string;
  notch: string;
  line: string;
  particle: string;
  head: string;
  headGlow: string;
}

/** Icy blue (light theme). */
export const ICE_WAVE: WaveTint = {
  baseline: "167, 240, 255",
  glow: "90, 200, 255",
  bar: (n, original) => `${Math.round(lerp(40, original ? 245 : 225, n))}, ${Math.round(lerp(120, 245, n))}, 255`,
  liveBar: (n) => `${Math.round(lerp(60, 225, n))}, ${Math.round(lerp(150, 245, n))}, 255`,
  rawOn: "205, 235, 255",
  rawOff: "120, 165, 210",
  selection: "57, 169, 255",
  handle: "167, 240, 255",
  handleGlow: "120, 220, 255",
  notch: "9, 33, 59",
  line: "57, 169, 255",
  particle: "190, 235, 255",
  head: "235, 250, 255",
  headGlow: "150, 230, 255",
};

/** White on black (dark theme). */
export const WHITE_WAVE: WaveTint = {
  baseline: "255, 255, 255",
  glow: "255, 255, 255",
  bar: (n, original) => {
    const v = Math.round(lerp(150, original ? 255 : 245, n));
    return `${v}, ${v}, ${Math.min(255, v + 4)}`;
  },
  liveBar: (n) => {
    const v = Math.round(lerp(160, 248, n));
    return `${v}, ${v}, ${Math.min(255, v + 4)}`;
  },
  rawOn: "240, 241, 243",
  rawOff: "150, 152, 158",
  selection: "255, 255, 255",
  handle: "245, 246, 248",
  handleGlow: "255, 255, 255",
  notch: "20, 21, 23",
  line: "255, 255, 255",
  particle: "235, 236, 240",
  head: "255, 255, 255",
  headGlow: "255, 255, 255",
};

function bucketMax(peaks: number[], from: number, to: number) {
  const n = peaks.length;
  const s0 = Math.floor(from * n);
  const s1 = Math.max(s0 + 1, Math.ceil(to * n));
  let m = 0;
  for (let i = s0; i < s1 && i < n; i++) m = Math.max(m, peaks[i]);
  return m;
}

export function xForFraction(f: number) {
  return PAD_X + f * INNER_W;
}

export function fractionForX(x: number) {
  return Math.min(1, Math.max(0, (x - PAD_X) / INNER_W));
}

export function drawBarLayer(ctx: CanvasRenderingContext2D, input: BarLayerInput, tint: WaveTint = ICE_WAVE) {
  ctx.clearRect(0, 0, WAVE_W, WAVE_H);
  const { view, peaks } = input;

  // Fine dotted baseline.
  ctx.fillStyle = `rgba(${tint.baseline}, 0.38)`;
  for (let x = PAD_X; x <= WAVE_W - PAD_X; x += 6) {
    ctx.beginPath();
    ctx.arc(x, CENTER_Y, 0.9, 0, Math.PI * 2);
    ctx.fill();
  }

  if (peaks.length === 0) return;

  const maxPeak = Math.max(1e-4, ...peaks);
  const spacing = view === "edit" ? 6.5 : 3;
  const barW = view === "edit" ? 2.6 : 1.5;
  const count = Math.floor(INNER_W / spacing);

  for (let i = 0; i < count; i++) {
    const f0 = i / count;
    const f1 = (i + 1) / count;
    const norm = Math.pow(bucketMax(peaks, f0, f1) / maxPeak, view === "edit" ? 0.85 : 0.7);
    const x = PAD_X + i * spacing + (spacing - barW) / 2;
    const half = norm * MAX_HALF;

    const inSelection = !input.selection || (f1 > input.selection[0] && f0 < input.selection[1]);
    const isOriginal = input.originalFraction !== null && f0 >= input.originalFraction;

    if (half < 1.4) continue;   // the dotted baseline already shows silence

    if (view === "edit") {
      ctx.fillStyle = `rgba(${tint.bar(norm, isOriginal)}, ${lerp(0.55, 1, norm)})`;
      ctx.shadowColor = `rgba(${tint.glow}, 0.95)`;
      ctx.shadowBlur = 3 + 11 * norm;
    } else {
      ctx.fillStyle = inSelection ? `rgba(${tint.rawOn}, 0.92)` : `rgba(${tint.rawOff}, 0.28)`;
      ctx.shadowColor = `rgba(${tint.glow}, 0.7)`;
      ctx.shadowBlur = inSelection ? 4 : 0;
    }

    ctx.beginPath();
    ctx.roundRect(x, CENTER_Y - half, barW, half * 2, barW / 2);
    ctx.fill();
  }

  ctx.shadowBlur = 0;

  if (view === "edit" && input.originalFraction !== null && input.originalFraction < 0.999) {
    const x = xForFraction(input.originalFraction);
    ctx.strokeStyle = "rgba(255, 255, 255, 0.28)";
    ctx.setLineDash([2, 4]);
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(x, CENTER_Y - MAX_HALF - 4);
    ctx.lineTo(x, CENTER_Y + MAX_HALF + 4);
    ctx.stroke();
    ctx.setLineDash([]);
  }

  if (view === "raw" && input.selection) {
    const [s0, s1] = input.selection;
    const x0 = xForFraction(s0);
    const x1 = xForFraction(s1);
    const top = CENTER_Y - MAX_HALF - 2;
    const bottom = CENTER_Y + MAX_HALF + 2;

    ctx.fillStyle = `rgba(${tint.selection}, 0.10)`;
    ctx.fillRect(x0, top, x1 - x0, bottom - top);

    ctx.shadowColor = `rgba(${tint.handleGlow}, 0.9)`;
    ctx.shadowBlur = 8;
    ctx.fillStyle = `rgb(${tint.handle})`;
    for (const x of [x0, x1]) {
      ctx.fillRect(x - 1, top, 2, bottom - top);
      ctx.beginPath();
      ctx.roundRect(x - 4.5, CENTER_Y - 13, 9, 26, 4);   // grab tab at the centre line
      ctx.fill();
    }
    ctx.shadowBlur = 0;
    ctx.fillStyle = `rgba(${tint.notch}, 0.85)`;
    for (const x of [x0, x1]) {
      ctx.fillRect(x - 1.5, CENTER_Y - 6, 1, 12);
      ctx.fillRect(x + 0.5, CENTER_Y - 6, 1, 12);
    }
    ctx.shadowBlur = 0;
  }
}

//==============================================================================
interface Particle { x: number; y: number; r: number; phase: number; speed: number }

const PARTICLES: Particle[] = (() => {
  let seed = 9;
  const rnd = () => ((seed = (seed * 16807) % 2147483647) / 2147483647);
  return Array.from({ length: 34 }, () => ({
    x: rnd() * WAVE_W,
    y: CENTER_Y - MAX_HALF + rnd() * MAX_HALF * 2,
    r: 0.5 + rnd() * 1.1,
    phase: rnd() * Math.PI * 2,
    speed: 3 + rnd() * 9,
  }));
})();

export function drawAtmosphere(ctx: CanvasRenderingContext2D, t: number, emphasis: number[], tint: WaveTint = ICE_WAVE) {
  ctx.clearRect(0, 0, WAVE_W, WAVE_H);

  // Liquid lines: low-opacity sines whose amplitude follows the waveform's energy.
  const energyAt = (x: number) => {
    if (emphasis.length === 0) return 0.3;
    const i = Math.min(emphasis.length - 1, Math.max(0, Math.floor(((x - PAD_X) / INNER_W) * emphasis.length)));
    return 0.25 + 0.75 * emphasis[i];
  };

  const lines = [
    { amp: 12, freq: 0.011, speed: 0.55, alpha: 0.16, width: 1.2, phase: 0 },
    { amp: 16, freq: 0.0074, speed: -0.38, alpha: 0.11, width: 1.6, phase: 1.9 },
    { amp: 8, freq: 0.017, speed: 0.8, alpha: 0.09, width: 1, phase: 4.1 },
    { amp: 20, freq: 0.0052, speed: 0.24, alpha: 0.07, width: 2.4, phase: 2.6 },
  ];

  for (const l of lines) {
    ctx.strokeStyle = `rgba(${tint.line}, ${l.alpha})`;
    ctx.lineWidth = l.width;
    ctx.beginPath();
    for (let x = 0; x <= WAVE_W; x += 8) {
      const y = CENTER_Y + Math.sin(x * l.freq + t * l.speed + l.phase) * l.amp * energyAt(x);
      if (x === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();
  }

  // Sparse luminous particles drifting right.
  for (const p of PARTICLES) {
    const x = (p.x + t * p.speed) % WAVE_W;
    const a = 0.12 + 0.28 * (0.5 + 0.5 * Math.sin(t * 0.9 + p.phase));
    ctx.fillStyle = `rgba(${tint.particle}, ${a})`;
    ctx.beginPath();
    ctx.arc(x, p.y + Math.sin(t * 0.4 + p.phase) * 3, p.r, 0, Math.PI * 2);
    ctx.fill();
  }
}

export function drawPlayhead(ctx: CanvasRenderingContext2D, fraction: number, active: boolean, tint: WaveTint = ICE_WAVE) {
  const x = xForFraction(fraction);
  ctx.fillStyle = active ? "rgba(255, 255, 255, 0.95)" : `rgba(${tint.head}, 0.75)`;
  ctx.shadowColor = `rgba(${tint.headGlow}, 0.95)`;
  ctx.shadowBlur = active ? 10 : 4;
  ctx.fillRect(x - 1, CENTER_Y - MAX_HALF - 4, 2, MAX_HALF * 2 + 8);
  ctx.shadowBlur = 0;
}

//==============================================================================
export const LIVE_SPACING = 6.5;
export const LIVE_BARS = Math.ceil(INNER_W / LIVE_SPACING) + 2;

/**
    Live input while the DAW is recording into the plugin: newest level at the right edge,
    scrolling left. `shift` (0..1) is how far the next bar has slid in, for smooth motion.
    Silence still shows a gentle travelling ripple so recording is visibly alive.
*/
export function drawLiveWave(
  ctx: CanvasRenderingContext2D, levels: number[], shift: number, t: number, tint: WaveTint = ICE_WAVE,
) {
  const right = PAD_X + INNER_W;
  const maxHalf = MAX_HALF;   // the band is already clear of the buttons
  const headHalf = MAX_HALF * 0.75;
  const recentMax = Math.max(0.03, ...levels.slice(-90));
  const barW = 2.6;

  for (let k = 0; k < levels.length; k++) {
    const age = levels.length - 1 - k; // 0 = newest
    const x = right - (age + shift) * LIVE_SPACING;
    if (x < PAD_X - LIVE_SPACING) continue;

    const norm = Math.min(1, levels[k] / recentMax);
    const ripple = 1.6 + 1.3 * Math.sin(t * 3.2 - k * 0.42);
    const half = Math.max(ripple, Math.pow(norm, 0.75) * maxHalf);
    const fadeIn = Math.min(1, (x - PAD_X) / 120); // older bars dissolve on the left
    const alpha = Math.max(0, fadeIn) * (0.45 + 0.55 * Math.max(norm, 0.25));

    ctx.fillStyle = `rgba(${tint.liveBar(norm)}, ${alpha})`;
    ctx.shadowColor = `rgba(${tint.glow}, 0.9)`;
    ctx.shadowBlur = 3 + 10 * norm;
    ctx.beginPath();
    ctx.roundRect(x - barW / 2, CENTER_Y - half, barW, half * 2, barW / 2);
    ctx.fill();
  }

  // Write head: a soft pulsing line where new audio arrives.
  const pulse = 0.55 + 0.45 * Math.sin(t * 4);
  const grad = ctx.createLinearGradient(right - 40, 0, right + 6, 0);
  grad.addColorStop(0, `rgba(${tint.baseline}, 0)`);
  grad.addColorStop(1, `rgba(${tint.baseline}, ${0.18 * pulse})`);
  ctx.shadowBlur = 0;
  ctx.fillStyle = grad;
  ctx.fillRect(right - 40, CENTER_Y - headHalf, 46, headHalf * 2);
  ctx.fillStyle = `rgba(${tint.head}, ${0.5 + 0.5 * pulse})`;
  ctx.shadowColor = `rgba(${tint.headGlow}, 1)`;
  ctx.shadowBlur = 12;
  ctx.fillRect(right + 4, CENTER_Y - headHalf, 2, headHalf * 2);
  ctx.shadowBlur = 0;
}
