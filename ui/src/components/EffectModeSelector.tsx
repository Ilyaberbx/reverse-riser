import { useEffect, useRef, useState } from "react";
import { EFFECTS } from "../params";
import { actions, useStore } from "../store";
import { HorizontalSlider } from "./HorizontalSlider";
import { HeadphonesIcon, PowerIcon } from "./icons";

const CLOSE_DELAY_MS = 260;

/**
    Phone / Chorus / Tremolo colour the swell. Each is an independent on/off switch: any
    combination can be on, and an effect that is on glows. Hovering (or focusing) an effect
    opens its settings flyout above it; adjusting settings never switches anything on or off.
*/
export function EffectModeSelector() {
  const params = useStore((s) => s.params);
  const [openSlot, setOpenSlot] = useState<number | null>(null);
  const closeTimer = useRef<number | undefined>(undefined);

  const open = (slot: number) => {
    window.clearTimeout(closeTimer.current);
    setOpenSlot(slot);
  };
  const scheduleClose = () => {
    window.clearTimeout(closeTimer.current);
    closeTimer.current = window.setTimeout(() => setOpenSlot(null), CLOSE_DELAY_MS);
  };
  useEffect(() => () => window.clearTimeout(closeTimer.current), []);

  const toggle = (id: (typeof EFFECTS)[number]["id"]) => {
    actions.beginGesture(id);
    actions.setParam(id, params[id] > 0.5 ? 0 : 1);
    actions.endGesture(id);
  };

  const openEffect = openSlot !== null ? EFFECTS[openSlot] : null;

  return (
    <div className="mode-selector-wrap" onPointerLeave={scheduleClose}>
      {openEffect && (
        <div
          className="mode-flyout"
          style={{ left: `${(openSlot! + 0.5) * (100 / 3)}%` }}
          onPointerEnter={() => open(openSlot!)}
          onFocus={() => open(openSlot!)}
          onBlur={scheduleClose}
          role="group"
          aria-label={`${openEffect.label.toLowerCase()} settings`}
        >
          {openEffect.settings.map((p) => (
            <HorizontalSlider key={p.id} param={p} dimmed={params[openEffect.id] < 0.5} />
          ))}
        </div>
      )}

      <div className="mode-selector" role="group" aria-label="Swell effects">
        {EFFECTS.map((effect, slot) => {
          const on = params[effect.id] > 0.5;
          const Icon = effect.icon === "headphones" ? HeadphonesIcon : PowerIcon;
          return (
            <button
              key={effect.id}
              type="button"
              aria-pressed={on}
              className={`mode-selector__option ${on ? "is-on" : ""}`}
              title={`${effect.label.toLowerCase()} ${on ? "on" : "off"}. Click to switch it ${on ? "off" : "on"}.`}
              onClick={() => toggle(effect.id)}
              onPointerEnter={() => open(slot)}
              onFocus={() => open(slot)}
              onBlur={scheduleClose}
            >
              <Icon size={17} strokeWidth={2.3} />
              <span>{effect.label}</span>
            </button>
          );
        })}
      </div>
    </div>
  );
}
