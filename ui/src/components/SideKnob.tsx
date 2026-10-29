import type { ContinuousParam } from "../params";
import { actions, useStore } from "../store";
import { RotaryKnob } from "./RotaryKnob";

/** A stacked knob bound to one parameter. Log parameters get an even feel across octaves. */
export function SideKnob({ param }: { param: ContinuousParam }) {
  const value = useStore((s) => s.params[param.id]);

  const toPos = (v: number) => (100 * Math.log(v / param.min)) / Math.log(param.max / param.min);
  const fromPos = (p: number) => {
    const v = param.min * Math.pow(param.max / param.min, p / 100);
    return Math.min(param.max, Math.max(param.min, Math.round(v / param.step) * param.step));
  };

  const common = {
    stacked: true,
    size: 54,
    label: param.label,
    onGestureStart: () => actions.beginGesture(param.id),
    onGestureEnd: () => actions.endGesture(param.id),
  };

  return param.log ? (
    <RotaryKnob
      {...common}
      value={toPos(value)}
      min={0}
      max={100}
      step={0.25}
      defaultValue={toPos(param.defaultValue)}
      format={(p) => param.format(fromPos(p))}
      onChange={(p) => actions.setParam(param.id, fromPos(p))}
    />
  ) : (
    <RotaryKnob
      {...common}
      value={value}
      min={param.min}
      max={param.max}
      step={param.step}
      defaultValue={param.defaultValue}
      format={param.format}
      onChange={(v) => actions.setParam(param.id, v)}
    />
  );
}
