import { useId } from "react";
import { arcPath, normalise, sweepAngle, useRotary } from "./useRotary";

export interface RotaryKnobProps {
  label: string;
  value: number;
  min: number;
  max: number;
  step?: number;
  defaultValue: number;
  format: (v: number) => string;
  onChange: (v: number) => void;
  onGestureStart?: () => void;
  onGestureEnd?: () => void;
  size?: number;
  /** Dial on top with the label and value centred below, instead of side by side. */
  stacked?: boolean;
}

/**
    Metallic rotary control: blue progress arc, dark rim, brushed silver cap and a pointer.
    The label and value readout sit to the right, as in the side panels.
*/
export function RotaryKnob({
  label,
  value,
  min,
  max,
  step = 1,
  defaultValue,
  format,
  onChange,
  onGestureStart,
  onGestureEnd,
  size = 66,
  stacked = false,
}: RotaryKnobProps) {
  const id = useId();
  const labelId = `${id}-label`;
  const norm = normalise(value, min, max);
  const angle = sweepAngle(norm);
  const { ref, dragging, handlers } = useRotary({
    value,
    min,
    max,
    step,
    defaultValue,
    onChange,
    onBegin: onGestureStart,
    onEnd: onGestureEnd,
  });

  const box = size + 14;
  const c = box / 2;
  const r = size / 2 + 3;

  return (
    <div className={`knob-row ${stacked ? "is-stacked" : ""}`}>
      <div
        ref={ref}
        className={`knob ${dragging ? "is-dragging" : ""}`}
        style={{ width: box, height: box }}
        role="slider"
        tabIndex={0}
        aria-labelledby={labelId}
        aria-valuemin={min}
        aria-valuemax={max}
        aria-valuenow={Math.round(value * 100) / 100}
        aria-valuetext={format(value)}
        title="Drag, scroll or use the arrow keys. Double-click to reset."
        {...handlers}
      >
        <svg className="knob__arc" width={box} height={box} aria-hidden="true">
          <defs>
            <linearGradient id={`${id}-g`} x1="0" y1="1" x2="1" y2="0">
              <stop offset="0" stopColor="var(--blue-primary)" />
              <stop offset="1" stopColor="var(--cyan-highlight)" />
            </linearGradient>
          </defs>
          <path d={arcPath(c, c, r, -135, 135)} className="knob__track" />
          {norm > 0.002 && (
            <path d={arcPath(c, c, r, -135, angle)} className="knob__value" stroke={`url(#${id}-g)`} />
          )}
        </svg>
        <div className="knob__rim" style={{ width: size, height: size }}>
          <div className="knob__cap" style={{ transform: `rotate(${angle}deg)` }}>
            <span className="knob__pointer" />
          </div>
        </div>
      </div>
      <div className="knob-text">
        <span id={labelId} className="knob-text__label">
          {label}
        </span>
        <span className="knob-text__value">{format(value)}</span>
      </div>
    </div>
  );
}
