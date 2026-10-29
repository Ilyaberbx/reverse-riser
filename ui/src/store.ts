import { useSyncExternalStore } from "react";
import { backend } from "./bridge";
import type { ParamId, Params, Playback, PluginState, Toast, Waveforms } from "./bridge";
import { DEFAULT_PARAMS } from "./params";

/**
    Single source of truth for the UI. Values come from the backend; user edits are applied
    optimistically and sent on. While a control is being dragged, echoes for that parameter
    are ignored so the knob never jumps back to a stale value.
*/
export interface AppStore {
  connected: boolean;
  state: PluginState;
  params: Params;
  waveforms: Waveforms;
  playback: Playback;
  toast: (Toast & { id: number }) | null;
}

let store: AppStore = {
  connected: false,
  state: {
    hasSource: false,
    sourceKind: "none",
    isStandalone: false,
    capture: "off",
    hasTimeline: false,
    phraseAt: "",
    swellAt: "",
    sibilantRegions: [],
    isDemo: false,
    fileName: "",
    sourceSeconds: 0,
    sourceRate: 44100,
    channels: 2,
    selStart: 0,
    selEnd: 0,
    rendered: null,
    presets: ["Default"],
    factoryPresetCount: 1,
    presetIndex: 0,
    presetModified: false,
    bpm: 120,
    tailSeconds: 2,
    loop: false,
    exportsFolder: "",
  },
  params: { ...DEFAULT_PARAMS },
  waveforms: { raw: [], edit: [], version: 0 },
  playback: { playing: false, source: "edit", position: 0, hostPlaying: false, recording: false, captureSeconds: 0, level: 0 },
  toast: null,
};

const subscribers = new Set<() => void>();
const activeGestures = new Set<ParamId>();
let toastId = 0;

function set(patch: Partial<AppStore>) {
  store = { ...store, ...patch };
  subscribers.forEach((fn) => fn());
}

function subscribe(fn: () => void) {
  subscribers.add(fn);
  return () => {
    subscribers.delete(fn);
  };
}

/** Selectors must return existing references or primitives (never build new objects here). */
export function useStore<T>(selector: (s: AppStore) => T): T {
  return useSyncExternalStore(subscribe, () => selector(store));
}

export const getStore = () => store;

export function showToast(text: string, error = false) {
  if (!text) return;
  set({ toast: { text, error, id: ++toastId } });
}

function mergeParams(incoming: Params) {
  const merged = { ...incoming };
  activeGestures.forEach((id) => (merged[id] = store.params[id]));
  set({ params: merged });
}

export const actions = {
  setParam(id: ParamId, value: number) {
    set({ params: { ...store.params, [id]: value } });
    backend.setParam(id, value);
  },
  beginGesture(id: ParamId) {
    activeGestures.add(id);
    backend.beginGesture(id);
  },
  endGesture(id: ParamId) {
    activeGestures.delete(id);
    backend.endGesture(id);
  },
  setLoop(loop: boolean) {
    set({ state: { ...store.state, loop } });
    backend.setLoop(loop);
  },
  setSelection(start: number, end: number) {
    set({ state: { ...store.state, selStart: start, selEnd: end } });
    backend.setSelection(start, end);
  },
};

export async function connect() {
  backend.on("glacier:state", (state) => set({ state }));
  backend.on("glacier:params", mergeParams);
  backend.on("glacier:waveforms", (waveforms) => set({ waveforms }));
  backend.on("glacier:playback", (playback) => set({ playback }));
  backend.on("glacier:toast", (t) => showToast(t.text, t.error));

  const initial = await backend.ready();
  set({ connected: true, ...initial });
}
