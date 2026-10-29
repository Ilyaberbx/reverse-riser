import type { ParamId, Params } from "./bridge/types";

/** UI metadata for the continuous parameters. Ranges mirror createParameterLayout() in C++. */
export interface ContinuousParam {
  id: ParamId;
  label: string;
  min: number;
  max: number;
  step: number;
  defaultValue: number;
  format: (v: number) => string;
}

const percent = (v: number) => `${Math.round(v)} %`;

export const FADE: ContinuousParam = {
  id: "fade",
  label: "FADE",
  min: 0,
  max: 100,
  step: 1,
  defaultValue: 100,
  format: percent,
};

export const SIZE: ContinuousParam = {
  id: "size",
  label: "SIZE",
  min: 10,
  max: 150,
  step: 1,
  defaultValue: 120,
  format: percent,
};

/** Settings of the swell character effects (shown in a flyout above each mode). */
export const PHONE_MIX: ContinuousParam = {
  id: "phoneMix", label: "MIX", min: 0, max: 100, step: 1, defaultValue: 100, format: percent,
};
export const PHONE_TONE: ContinuousParam = {
  id: "phoneTone", label: "TONE", min: 0, max: 100, step: 1, defaultValue: 50,
  format: (v) => (v < 34 ? "Dark" : v < 67 ? "Mid" : "Bright") + ` ${Math.round(v)}`,
};
export const CHORUS_MIX: ContinuousParam = {
  id: "chorusMix", label: "MIX", min: 0, max: 100, step: 1, defaultValue: 60, format: percent,
};
export const CHORUS_RATE: ContinuousParam = {
  id: "chorusRate", label: "RATE", min: 0.1, max: 5, step: 0.01, defaultValue: 0.8,
  format: (v) => `${v.toFixed(2)} Hz`,
};
export const TREMOLO_MIX: ContinuousParam = {
  id: "tremoloMix", label: "MIX", min: 0, max: 100, step: 1, defaultValue: 100, format: percent,
};
export const TREMOLO_TIMINGS = ["1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T"];
export const TREMOLO_TIMING: ContinuousParam = {
  id: "tremoloTiming", label: "TIMING", min: 0, max: TREMOLO_TIMINGS.length - 1, step: 1, defaultValue: 1,
  format: (v) => TREMOLO_TIMINGS[Math.round(v)] ?? "1/8",
};

/** Musical lengths of the swell; the orb steps through these. */
export const REVERSE_CHOICES = ["1/8 BAR", "1/4 BAR", "1/2 BAR", "1 BAR", "2 BARS", "4 BARS", "8 BARS"];
export const REVERSE_BARS = [0.125, 0.25, 0.5, 1, 2, 4, 8];
export const REVERSE_DEFAULT = 3;

/** Swell effects. Each is an independent on/off switch with two settings. */
export const EFFECTS = [
  { id: "phoneOn", label: "PHONE", icon: "headphones", settings: [PHONE_MIX, PHONE_TONE] },
  { id: "chorusOn", label: "CHORUS", icon: "power", settings: [CHORUS_MIX, CHORUS_RATE] },
  { id: "tremoloOn", label: "TREMOLO", icon: "power", settings: [TREMOLO_MIX, TREMOLO_TIMING] },
] as const;

export const DEFAULT_PARAMS: Params = {
  reverse: REVERSE_DEFAULT,
  fade: FADE.defaultValue,
  size: SIZE.defaultValue,
  phoneOn: 0,
  chorusOn: 1,
  tremoloOn: 0,
  includeOriginal: 0,
  phoneMix: PHONE_MIX.defaultValue,
  phoneTone: PHONE_TONE.defaultValue,
  chorusMix: CHORUS_MIX.defaultValue,
  chorusRate: CHORUS_RATE.defaultValue,
  tremoloMix: TREMOLO_MIX.defaultValue,
  tremoloTiming: TREMOLO_TIMING.defaultValue,
};

export function formatTime(seconds: number): string {
  const total = Math.max(0, seconds);
  const m = Math.floor(total / 60);
  const s = total - m * 60;
  return `${m}:${s.toFixed(3).padStart(6, "0")}`;
}
