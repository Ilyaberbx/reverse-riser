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

export function drawVortex(ctx: CanvasRenderingContext2D, size: number, rotation: number, amount: number) {
  const R = size / 2;
  ctx.save();
  ctx.clearRect(0, 0, size, size);
  ctx.translate(R, R);

  // Deep water body.
  const body = ctx.createRadialGradient(0, 0, 0, 0, 0, R);
  body.addColorStop(0, "#01050d");
  body.addColorStop(0.18, "#031230");
  body.addColorStop(0.5, "#08306a");
  body.addColorStop(0.82, "#0d4f9e");
  body.addColorStop(1, "#0a2c5c");
  ctx.fillStyle = body;
  ctx.beginPath();
  ctx.arc(0, 0, R, 0, Math.PI * 2);
  ctx.fill();

  // Filaments.
  const glow = ctx.createRadialGradient(0, 0, R * 0.04, 0, 0, R);
  glow.addColorStop(0, "rgba(120, 200, 255, 0)");
  glow.addColorStop(0.22, "rgba(110, 200, 255, 0.55)");
  glow.addColorStop(0.55, "rgba(167, 240, 255, 0.95)");
  glow.addColorStop(0.85, "rgba(80, 170, 255, 0.85)");
  glow.addColorStop(1, "rgba(40, 120, 255, 0.2)");

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
  hole.addColorStop(0, "rgba(0, 4, 12, 0.97)");
  hole.addColorStop(0.5, "rgba(1, 10, 28, 0.62)");
  hole.addColorStop(1, "rgba(1, 10, 28, 0)");
  ctx.fillStyle = hole;
  ctx.beginPath();
  ctx.arc(0, 0, holeR, 0, Math.PI * 2);
  ctx.fill();

  // A faint icy rim marks the edge of the opening, so its narrowing and widening reads clearly.
  const rimIn = R * eye * 0.8, rimOut = R * eye * 1.3;
  const eyeRim = ctx.createRadialGradient(0, 0, rimIn, 0, 0, rimOut);
  eyeRim.addColorStop(0, "rgba(167, 240, 255, 0)");
  eyeRim.addColorStop(0.5, "rgba(167, 240, 255, 0.16)");
  eyeRim.addColorStop(1, "rgba(167, 240, 255, 0)");
  ctx.globalCompositeOperation = "lighter";
  ctx.fillStyle = eyeRim;
  ctx.beginPath();
  ctx.arc(0, 0, rimOut, 0, Math.PI * 2);
  ctx.fill();
  ctx.globalCompositeOperation = "source-over";

  // Rim shadow for a concave feel.
  const rim = ctx.createRadialGradient(0, 0, R * 0.78, 0, 0, R);
  rim.addColorStop(0, "rgba(2, 10, 26, 0)");
  rim.addColorStop(1, "rgba(2, 10, 26, 0.75)");
  ctx.fillStyle = rim;
  ctx.beginPath();
  ctx.arc(0, 0, R, 0, Math.PI * 2);
  ctx.fill();

  // Soft white-blue highlight in the upper left.
  const hi = ctx.createRadialGradient(-R * 0.38, -R * 0.45, 0, -R * 0.38, -R * 0.45, R * 0.6);
  hi.addColorStop(0, "rgba(220, 245, 255, 0.28)");
  hi.addColorStop(1, "rgba(220, 245, 255, 0)");
  ctx.fillStyle = hi;
  ctx.beginPath();
  ctx.arc(0, 0, R, 0, Math.PI * 2);
  ctx.fill();

  ctx.restore();
}
