import { useRef } from "react";
import { REVERSE_BARS, REVERSE_CHOICES, REVERSE_DEFAULT } from "../params";
import { sizeCanvas, useAnimationFrame, usePrefersReducedMotion } from "./useAnimationFrame";
import { arcPath, normalise, sweepAngle, useRotary } from "./useRotary";
import { useTheme } from "../theme";
import { drawVortex, ICE_VORTEX, WHITE_VORTEX } from "./vortex";

const BOX = 296;          // full control including the tick scale
const C = BOX / 2;
const VORTEX = 214;       // diameter of the vortex canvas

interface ReverseOrbProps {
  value: number;                 // index into REVERSE_CHOICES
  tailSeconds: number;           // swell length at the current tempo
  onChange: (index: number) => void;
  onGestureStart: () => void;
  onGestureEnd: () => void;
}

/** The large central control: sets the musical length of the reverse swell. */
export function ReverseOrb({ value, tailSeconds, onChange, onGestureStart, onGestureEnd }: ReverseOrbProps) {
  const max = REVERSE_CHOICES.length - 1;
  const norm = normalise(value, 0, max);
  const angle = sweepAngle(norm);
  const canvasRef = useRef<HTMLCanvasElement>(null);
  // Motion state lives across frames: the swirl angle is integrated, never recomputed from the
  // clock, so turning the knob changes how fast it turns without making it jump or race.
  const motion = useRef({ amount: norm, rotation: 0, last: -1 });
  const reducedMotion = usePrefersReducedMotion();
  const palette = useTheme() === "dark" ? WHITE_VORTEX : ICE_VORTEX;

  const { ref, dragging, handlers } = useRotary({
    value,
    min: 0,
    max,
    step: 1,
    defaultValue: REVERSE_DEFAULT,
    dragRange: max * 34,
    onChange,
    onBegin: onGestureStart,
    onEnd: onGestureEnd,
  });

  useAnimationFrame(
    (t) => {
      const canvas = canvasRef.current;
      if (!canvas) return;
      const m = motion.current;
      const dt = m.last < 0 ? 0 : Math.min(0.05, Math.max(0, t - m.last)); // cap gaps (hidden window, stalls)
      m.last = t;

      if (reducedMotion) m.amount = norm;
      else m.amount += (norm - m.amount) * (1 - Math.exp(-dt / 0.6)); // ~0.6 s ease at any frame rate

      m.rotation += dt * (0.42 - m.amount * 0.22); // radians per second
      drawVortex(sizeCanvas(canvas, VORTEX, VORTEX), VORTEX, m.rotation, m.amount, palette);
    },
    !reducedMotion,
    [norm, palette],
  );

  // Scale: fine dots along the sweep, larger dots at each musical length.
  const ticks = [];
  const fine = 48;
  for (let i = 0; i <= fine; i++) {
    const a = -135 + (270 * i) / fine;
    const rad = ((a - 90) * Math.PI) / 180;
    const lit = a <= angle + 0.01;
    ticks.push(
      <circle key={`f${i}`} cx={C + 136 * Math.cos(rad)} cy={C + 136 * Math.sin(rad)} r={1.15}
        className={lit ? "orb-tick is-lit" : "orb-tick"} />,
    );
  }
  for (let i = 0; i <= max; i++) {
    const a = sweepAngle(i / max);
    const rad = ((a - 90) * Math.PI) / 180;
    ticks.push(
      <circle key={`m${i}`} cx={C + 145 * Math.cos(rad)} cy={C + 145 * Math.sin(rad)} r={i === value ? 3.4 : 2.3}
        className={i <= value ? "orb-step is-lit" : "orb-step"} />,
    );
  }

  const bars = REVERSE_BARS[value] ?? 1;
  const [word, ...rest] = (REVERSE_CHOICES[value] ?? "").split(" ");

  return (
    <div className="orb">
      <div className="orb__glow" />
      <div
        ref={ref}
        className={`orb__control ${dragging ? "is-dragging" : ""}`}
        style={{ width: BOX, height: BOX }}
        role="slider"
        tabIndex={0}
        aria-label="Reverse length"
        aria-valuemin={0}
        aria-valuemax={max}
        aria-valuenow={value}
        aria-valuetext={`${REVERSE_CHOICES[value]}, ${tailSeconds.toFixed(2)} seconds`}
        title="Drag up or down, scroll, or use the arrow keys. Double-click for 1 bar."
        {...handlers}
      >
        <svg className="orb__scale" width={BOX} height={BOX} aria-hidden="true">
          <path d={arcPath(C, C, 145, -135, 135)} className="orb-scale-line" />
          {ticks}
        </svg>
        <div className="orb__metal" />
        <div className="orb__ring" />
        <div className="orb__well">
          <canvas ref={canvasRef} className="orb__vortex" style={{ width: VORTEX, height: VORTEX }} />
        </div>
        <svg className="orb__indicator" width={BOX} height={BOX} aria-hidden="true"
          style={{ transform: `rotate(${angle}deg)` }}>
          <line x1={C} y1={C - 106} x2={C} y2={C - 70} />
        </svg>
      </div>
      <div className="orb__label" aria-hidden="true" data-bars={bars}>
        REVERSE <span className="orb__label-value">{word} {rest.join(" ")}</span>
      </div>
    </div>
  );
}
