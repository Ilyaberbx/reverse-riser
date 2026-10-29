import { useEffect, useRef, useState } from "react";
import type { KeyboardEvent, PointerEvent } from "react";
import type { ContinuousParam } from "../params";
import { actions, useStore } from "../store";
import { clamp, normalise, snap } from "./useRotary";

/**
    Compact horizontal slider for the effect flyouts: label and value on top, a glowing track
    below. Drag or click the track, scroll, use the arrow keys, double-click to reset.
*/
export function HorizontalSlider({ param, dimmed = false }: { param: ContinuousParam; dimmed?: boolean }) {
  const value = useStore((s) => s.params[param.id]);
  const trackRef = useRef<HTMLDivElement>(null);
  const [dragging, setDragging] = useState(false);
  const latest = useRef(value);
  latest.current = value;

  const commit = (v: number) => {
    const next = snap(v, param.min, param.max, param.step);
    if (next !== latest.current) actions.setParam(param.id, next);
  };

  const valueAt = (clientX: number) => {
    const rect = trackRef.current!.getBoundingClientRect();
    return param.min + clamp((clientX - rect.left) / rect.width, 0, 1) * (param.max - param.min);
  };

  const onPointerDown = (e: PointerEvent<HTMLDivElement>) => {
    if (e.button !== 0) return;
    e.currentTarget.setPointerCapture(e.pointerId);
    e.currentTarget.focus({ preventScroll: true });
    setDragging(true);
    actions.beginGesture(param.id);
    commit(valueAt(e.clientX));
  };

  const onPointerMove = (e: PointerEvent<HTMLDivElement>) => {
    if (dragging) commit(valueAt(e.clientX));
  };

  const onPointerUp = (e: PointerEvent<HTMLDivElement>) => {
    if (!dragging) return;
    setDragging(false);
    if (e.currentTarget.hasPointerCapture(e.pointerId)) e.currentTarget.releasePointerCapture(e.pointerId);
    actions.endGesture(param.id);
  };

  const once = (v: number) => {
    actions.beginGesture(param.id);
    commit(v);
    actions.endGesture(param.id);
  };

  const onKeyDown = (e: KeyboardEvent<HTMLDivElement>) => {
    const big = Math.max(param.step, (param.max - param.min) / 10);
    const map: Record<string, number> = {
      ArrowRight: value + (e.shiftKey ? big : param.step),
      ArrowUp: value + (e.shiftKey ? big : param.step),
      ArrowLeft: value - (e.shiftKey ? big : param.step),
      ArrowDown: value - (e.shiftKey ? big : param.step),
      Home: param.min,
      End: param.max,
    };
    if (!(e.key in map)) return;
    e.preventDefault();
    once(map[e.key]);
  };

  useEffect(() => {
    const el = trackRef.current?.parentElement;
    if (!el) return;
    const onWheel = (e: WheelEvent) => {
      e.preventDefault();
      const amount = param.step >= (param.max - param.min) / 20 ? param.step : (param.max - param.min) / 50;
      once(latest.current + (e.deltaY < 0 ? amount : -amount));
    };
    el.addEventListener("wheel", onWheel, { passive: false });
    return () => el.removeEventListener("wheel", onWheel);
  }); // eslint-disable-line react-hooks/exhaustive-deps

  const pct = normalise(value, param.min, param.max) * 100;

  return (
    <div className={`hslider ${dimmed ? "is-dimmed" : ""}`}>
      <div className="hslider__head">
        <span className="hslider__label">{param.label}</span>
        <span className="hslider__value">{param.format(value)}</span>
      </div>
      <div
        className={`hslider__hit ${dragging ? "is-dragging" : ""}`}
        role="slider"
        tabIndex={0}
        aria-label={param.label}
        aria-valuemin={param.min}
        aria-valuemax={param.max}
        aria-valuenow={Math.round(value * 100) / 100}
        aria-valuetext={param.format(value)}
        onPointerDown={onPointerDown}
        onPointerMove={onPointerMove}
        onPointerUp={onPointerUp}
        onPointerCancel={onPointerUp}
        onDoubleClick={() => once(param.defaultValue)}
        onKeyDown={onKeyDown}
      >
        <div className="hslider__track" ref={trackRef}>
          <div className="hslider__fill" style={{ width: `${pct}%` }} />
          <div className="hslider__thumb" style={{ left: `${pct}%` }} />
        </div>
      </div>
    </div>
  );
}
