import { useCallback, useEffect, useRef, useState } from "react";
import type { KeyboardEvent, PointerEvent } from "react";

export interface RotaryOptions {
  value: number;
  min: number;
  max: number;
  step: number;
  defaultValue: number;
  /** Vertical drag distance (design px) that sweeps the whole range. */
  dragRange?: number;
  onChange: (v: number) => void;
  onBegin?: () => void;
  onEnd?: () => void;
}

export const clamp = (v: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, v));

export function snap(v: number, min: number, max: number, step: number) {
  return clamp(min + Math.round((v - min) / step) * step, min, max);
}

/** 0..1 position of a value inside its range. */
export const normalise = (v: number, min: number, max: number) => (max > min ? (v - min) / (max - min) : 0);

/** Pointer angle for a rotary control with a 270 degree sweep, -135 at minimum. */
export const sweepAngle = (norm: number) => -135 + 270 * clamp(norm, 0, 1);

/**
    Drag vertically, scroll, use the keyboard, or double-click to reset.
    Shift gives fine control while dragging and coarse steps on the keyboard.
*/
export function useRotary({
  value,
  min,
  max,
  step,
  defaultValue,
  dragRange = 220,
  onChange,
  onBegin,
  onEnd,
}: RotaryOptions) {
  const ref = useRef<HTMLDivElement>(null);
  const drag = useRef<{ startY: number; startValue: number } | null>(null);
  const [dragging, setDragging] = useState(false);
  const latest = useRef({ value, onChange, onBegin, onEnd });
  latest.current = { value, onChange, onBegin, onEnd };

  const commit = useCallback(
    (v: number) => {
      const next = snap(v, min, max, step);
      if (next !== latest.current.value) latest.current.onChange(next);
    },
    [min, max, step],
  );

  const onPointerDown = (e: PointerEvent<HTMLDivElement>) => {
    if (e.button !== 0) return;
    e.currentTarget.setPointerCapture(e.pointerId);
    e.currentTarget.focus({ preventScroll: true });
    drag.current = { startY: e.clientY, startValue: value };
    setDragging(true);
    onBegin?.();
  };

  const onPointerMove = (e: PointerEvent<HTMLDivElement>) => {
    if (!drag.current) return;
    // clientY is in screen px; the stage is scaled, so convert back to design px.
    const scale = e.currentTarget.getBoundingClientRect().width / e.currentTarget.offsetWidth || 1;
    const dy = (drag.current.startY - e.clientY) / scale;
    const fine = e.shiftKey ? 0.2 : 1;
    commit(drag.current.startValue + (dy / dragRange) * (max - min) * fine);
  };

  const finishDrag = (e: PointerEvent<HTMLDivElement>) => {
    if (!drag.current) return;
    drag.current = null;
    setDragging(false);
    if (e.currentTarget.hasPointerCapture(e.pointerId)) e.currentTarget.releasePointerCapture(e.pointerId);
    onEnd?.();
  };

  const onDoubleClick = () => {
    onBegin?.();
    commit(defaultValue);
    onEnd?.();
  };

  const onKeyDown = (e: KeyboardEvent<HTMLDivElement>) => {
    const big = Math.max(step, (max - min) / 10);
    let next: number | null = null;
    switch (e.key) {
      case "ArrowUp":
      case "ArrowRight":
        next = value + (e.shiftKey ? big : step);
        break;
      case "ArrowDown":
      case "ArrowLeft":
        next = value - (e.shiftKey ? big : step);
        break;
      case "PageUp":
        next = value + big;
        break;
      case "PageDown":
        next = value - big;
        break;
      case "Home":
        next = min;
        break;
      case "End":
        next = max;
        break;
      case "Delete":
      case "Backspace":
        next = defaultValue;
        break;
    }
    if (next === null) return;
    e.preventDefault();
    onBegin?.();
    commit(next);
    onEnd?.();
  };

  // Wheel needs a non-passive listener so the page never scrolls instead.
  useEffect(() => {
    const el = ref.current;
    if (!el) return;
    let endTimer: number | undefined;
    let inGesture = false;

    const onWheel = (e: WheelEvent) => {
      e.preventDefault();
      if (!inGesture) {
        inGesture = true;
        latest.current.onBegin?.();
      }
      const direction = e.deltaY < 0 ? 1 : -1;
      const amount = step >= (max - min) / 20 ? step : Math.max(step, (max - min) / 100);
      commit(latest.current.value + direction * amount);
      window.clearTimeout(endTimer);
      endTimer = window.setTimeout(() => {
        inGesture = false;
        latest.current.onEnd?.();
      }, 250);
    };

    el.addEventListener("wheel", onWheel, { passive: false });
    return () => {
      el.removeEventListener("wheel", onWheel);
      window.clearTimeout(endTimer);
    };
  }, [commit, min, max, step]);

  return {
    ref,
    dragging,
    handlers: {
      onPointerDown,
      onPointerMove,
      onPointerUp: finishDrag,
      onPointerCancel: finishDrag,
      onDoubleClick,
      onKeyDown,
    },
  };
}

/** SVG arc path between two angles (degrees, 0 = 12 o'clock, clockwise). */
export function arcPath(cx: number, cy: number, r: number, fromDeg: number, toDeg: number) {
  const toXY = (deg: number) => {
    const rad = ((deg - 90) * Math.PI) / 180;
    return [cx + r * Math.cos(rad), cy + r * Math.sin(rad)];
  };
  const [x1, y1] = toXY(fromDeg);
  const [x2, y2] = toXY(toDeg);
  const large = Math.abs(toDeg - fromDeg) > 180 ? 1 : 0;
  return `M ${x1} ${y1} A ${r} ${r} 0 ${large} 1 ${x2} ${y2}`;
}
