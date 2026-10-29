import type { ContinuousParam } from "../params";
import { actions, useStore } from "../store";
import { RotaryKnob } from "./RotaryKnob";

/** A stacked knob bound to one parameter, placed beside the reverse orb. */
export function SideKnob({ param, side }: { param: ContinuousParam; side: "left" | "right" }) {
  const value = useStore((s) => s.params[param.id]);

  return (
    <div className={`side-knob side-knob--${side}`}>
      <RotaryKnob
        stacked
        size={62}
        label={param.label}
        value={value}
        min={param.min}
        max={param.max}
        step={param.step}
        defaultValue={param.defaultValue}
        format={param.format}
        onChange={(v) => actions.setParam(param.id, v)}
        onGestureStart={() => actions.beginGesture(param.id)}
        onGestureEnd={() => actions.endGesture(param.id)}
      />
    </div>
  );
}
