/**
    Canvas renderer for the fluid vortex inside the orb: logarithmic-spiral filaments drawn
    additively over a deep radial gradient. `rotation` is the accumulated swirl angle in radians:
    the caller integrates it, so a change of speed never makes it jump. `amount` (0..1, eased by
    the caller) follows the reverse length: short lengths open a wide eye with loose swirls,
    long lengths close it to a narrow eye with the filaments drawn tight.
*/

interface Filament {
  angle: number;
  startR: number;
  endU: number;     // 0..1: where inside the eye opening this filament ends
  twist: number;
  width: number;
  alpha: number;
  speed: number;
}

function makeFilaments(count: number): Filament[] {
  let seed = 42;
  const rnd = () => {
    seed = (seed * 16807) % 2147483647;
    return seed / 2147483647;
  };

  return Array.from({ length: count }, () => ({
    angle: rnd() * Math.PI * 2,
    startR: 0.55 + rnd() * 0.45,
    endU: rnd(),
    twist: 0.8 + rnd() * 0.9,
    width: 0.35 + rnd() * rnd() * 2.2,
    alpha: 0.12 + rnd() * 0.55,
    speed: 0.75 + rnd() * 0.5,
  }));
}

const FILAMENTS = makeFilaments(110);
const STEPS = 46;

type Stops = [number, string][];

/** Colours of the vortex. `eyeRim` and `highlight` are "r, g, b" triples (alpha is set in place). */
export interface VortexPalette {
  body: Stops;
  glow: Stops;
  hole: [string, string, string];
  eyeRim: string;
  rimShadow: string;
  highlight: string;
}

/** Blue water and icy filaments (light theme). */
export const ICE_VORTEX: VortexPalette = {
  body: [[0, "#01050d"], [0.18, "#031230"], [0.5, "#08306a"], [0.82, "#0d4f9e"], [1, "#0a2c5c"]],
  glow: [
    [0, "rgba(120, 200, 255, 0)"],
    [0.22, "rgba(110, 200, 255, 0.55)"],
    [0.55, "rgba(167, 240, 255, 0.95)"],
    [0.85, "rgba(80, 170, 255, 0.85)"],
    [1, "rgba(40, 120, 255, 0.2)"],
  ],
  hole: ["rgba(0, 4, 12, 0.97)", "rgba(1, 10, 28, 0.62)", "rgba(1, 10, 28, 0)"],
  eyeRim: "167, 240, 255",
  rimShadow: "2, 10, 26",
  highlight: "220, 245, 255",
};

/** White filaments on a black well (dark theme). */
export const WHITE_VORTEX: VortexPalette = {
  body: [[0, "#020202"], [0.18, "#060607"], [0.5, "#111214"], [0.82, "#1c1d20"], [1, "#101113"]],
  glow: [
    [0, "rgba(255, 255, 255, 0)"],
    [0.22, "rgba(235, 237, 240, 0.5)"],
    [0.55, "rgba(255, 255, 255, 0.95)"],
    [0.85, "rgba(220, 223, 228, 0.8)"],
    [1, "rgba(200, 204, 210, 0.2)"],
  ],
  hole: ["rgba(0, 0, 0, 0.97)", "rgba(2, 2, 3, 0.62)", "rgba(2, 2, 3, 0)"],
  eyeRim: "255, 255, 255",
  rimShadow: "0, 0, 0",
  highlight: "255, 255, 255",
};

const gradient = (g: CanvasGradient, stops: Stops) => {
  for (const [at, colour] of stops) g.addColorStop(at, colour);
  return g;
};

export function drawVortex(
  ctx: CanvasRenderingContext2D, size: number, rotation: number, amount: number, palette: VortexPalette = ICE_VORTEX,
) {
  const R = size / 2;
  ctx.save();
  ctx.clearRect(0, 0, size, size);
  ctx.translate(R, R);

  // Deep water body.
  ctx.fillStyle = gradient(ctx.createRadialGradient(0, 0, 0, 0, 0, R), palette.body);
  ctx.beginPath();
  ctx.arc(0, 0, R, 0, Math.PI * 2);
  ctx.fill();

  // Filaments.
  const glow = gradient(ctx.createRadialGradient(0, 0, R * 0.04, 0, 0, R), palette.glow);

  ctx.globalCompositeOperation = "lighter";
  ctx.strokeStyle = glow;
  ctx.lineCap = "round";

  const twistScale = 2.0 + amount * 1.8;
  const eye = 0.36 + (0.08 - 0.36) * amount;   // radius of the opening, as a fraction of R

  for (const f of FILAMENTS) {
    const r0 = f.startR * R;
    const r1 = R * eye * (0.7 + f.endU * 1.1);
    ctx.globalAlpha = f.alpha;
    ctx.lineWidth = f.width;
    ctx.beginPath();

    for (let i = 0; i <= STEPS; i++) {
      const u = i / STEPS;
      const r = r0 + (r1 - r0) * u;
      const theta = f.angle + rotation * f.speed + f.twist * twistScale * Math.log(r0 / r);
      const x = r * Math.cos(theta);
      const y = r * Math.sin(theta);
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();
  }

  ctx.globalAlpha = 1;
  ctx.globalCompositeOperation = "source-over";

  // Central depth.
  const holeR = R * eye * 1.45;
  const hole = ctx.createRadialGradient(0, 0, 0, 0, 0, holeR);
  hole.addColorStop(0, palette.hole[0]);
  hole.addColorStop(0.5, palette.hole[1]);
  hole.addColorStop(1, palette.hole[2]);
  ctx.fillStyle = hole;
  ctx.beginPath();
  ctx.arc(0, 0, holeR, 0, Math.PI * 2);
  ctx.fill();

  // A faint icy rim marks the edge of the opening, so its narrowing and widening reads clearly.
  const rimIn = R * eye * 0.8, rimOut = R * eye * 1.3;
  const eyeRim = ctx.createRadialGradient(0, 0, rimIn, 0, 0, rimOut);
  eyeRim.addColorStop(0, `rgba(${palette.eyeRim}, 0)`);
  eyeRim.addColorStop(0.5, `rgba(${palette.eyeRim}, 0.16)`);
  eyeRim.addColorStop(1, `rgba(${palette.eyeRim}, 0)`);
  ctx.globalCompositeOperation = "lighter";
  ctx.fillStyle = eyeRim;
  ctx.beginPath();
  ctx.arc(0, 0, rimOut, 0, Math.PI * 2);
  ctx.fill();
  ctx.globalCompositeOperation = "source-over";

  // Rim shadow for a concave feel.
  const rim = ctx.createRadialGradient(0, 0, R * 0.78, 0, 0, R);
  rim.addColorStop(0, `rgba(${palette.rimShadow}, 0)`);
  rim.addColorStop(1, `rgba(${palette.rimShadow}, 0.75)`);
  ctx.fillStyle = rim;
  ctx.beginPath();
  ctx.arc(0, 0, R, 0, Math.PI * 2);
  ctx.fill();

  // Soft white-blue highlight in the upper left.
  const hi = ctx.createRadialGradient(-R * 0.38, -R * 0.45, 0, -R * 0.38, -R * 0.45, R * 0.6);
  hi.addColorStop(0, `rgba(${palette.highlight}, 0.28)`);
  hi.addColorStop(1, `rgba(${palette.highlight}, 0)`);
  ctx.fillStyle = hi;
  ctx.beginPath();
  ctx.arc(0, 0, R, 0, Math.PI * 2);
  ctx.fill();

  ctx.restore();
}
