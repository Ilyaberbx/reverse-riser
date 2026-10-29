import { DEFAULT_PARAMS, REVERSE_BARS } from "../params";
import type {
  Backend,
  BackendEventName,
  BackendEvents,
  InitialPayload,
  Params,
  Playback,
  PluginState,
  Result,
  Waveforms,
} from "./types";

/**
    Browser-only stand-in for the plugin, so the interface can be developed and reviewed with
    `npm run dev`. It keeps the same state model as the C++ side and fakes the rendered waveform
    with an envelope that follows the same parameters. No audio is produced here.
*/

const BUCKETS = 720;

const FACTORY: { name: string; values: Partial<Params> }[] = [
  { name: "Default", values: {} },
  { name: "Short Snap", values: { reverse: 1, fade: 60, size: 80, chorusOn: 0 } },
  { name: "Ghost Vocal", values: { reverse: 4, fade: 100, size: 140, chorusMix: 75, chorusRate: 0.5 } },
  { name: "Old Radio", values: { reverse: 3, fade: 90, size: 100, phoneOn: 1, chorusOn: 0, phoneTone: 35 } },
  { name: "Pulse Riser", values: { reverse: 4, fade: 100, size: 130, tremoloOn: 1, chorusOn: 0, tremoloTiming: 2 } },
  { name: "Glacier", values: { reverse: 5, fade: 100, size: 150, chorusOn: 0 } },
  { name: "With Original", values: { includeOriginal: 1 } },
];

function seededNoise(seed: number) {
  let s = seed >>> 0;
  return () => {
    s = (s * 1664525 + 1013904223) >>> 0;
    return s / 0xffffffff;
  };
}

function demoPeaks(): number[] {
  const rnd = seededNoise(7);
  return Array.from({ length: BUCKETS }, (_, i) => {
    const t = (i / BUCKETS) * 1.8;
    const attack = Math.min(1, t / 0.006);
    return Math.min(1, 0.95 * attack * Math.exp(-t * 2.6) * (0.82 + 0.18 * rnd()));
  });
}

function peaksFromAudioBuffer(buffer: AudioBuffer): number[] {
  const n = buffer.length;
  const out = new Array<number>(BUCKETS).fill(0);
  for (let ch = 0; ch < buffer.numberOfChannels; ch++) {
    const data = buffer.getChannelData(ch);
    for (let b = 0; b < BUCKETS; b++) {
      const s0 = Math.floor((n * b) / BUCKETS);
      const s1 = Math.max(s0 + 1, Math.floor((n * (b + 1)) / BUCKETS));
      let m = 0;
      for (let s = s0; s < s1 && s < n; s++) m = Math.max(m, Math.abs(data[s]));
      out[b] = Math.max(out[b], m);
    }
  }
  return out;
}

export function createMockBackend(): Backend {
  const listeners = new Map<BackendEventName, Set<(p: never) => void>>();
  const userPresets: { name: string; values: Params }[] = [];

  let params: Params = { ...DEFAULT_PARAMS };
  let rawPeaks = demoPeaks();
  let waveformVersion = 1;
  let playback: Playback = {
    playing: false, source: "edit", position: 0, hostPlaying: false, recording: false, captureSeconds: 0, level: 0,
  };
  let playTimer: number | undefined;
  let captureTimer: number | undefined;
  let playStartedAt = 0;

  const state: PluginState = {
    hasSource: true,
    sourceKind: "demo",
    isStandalone: false,
    capture: "off",
    hasTimeline: false,
    phraseAt: "",
    swellAt: "",
    sibilantRegions: [],
    isDemo: true,
    fileName: "Demo pluck",
    sourceSeconds: 1.8,
    sourceRate: 44100,
    channels: 2,
    selStart: 0,
    selEnd: 1.8,
    rendered: null,
    presets: FACTORY.map((p) => p.name),
    factoryPresetCount: FACTORY.length,
    presetIndex: 0,
    presetModified: false,
    bpm: 120,
    tailSeconds: 2,
    loop: false,
    exportsFolder: "Documents\\Glacier Riser Exports",
  };

  let editPeaks: number[] = [];

  function emit<E extends BackendEventName>(event: E, payload: BackendEvents[E]) {
    listeners.get(event)?.forEach((fn) => (fn as (p: BackendEvents[E]) => void)(payload));
  }

  const toast = (text: string, error = false) => emit("glacier:toast", { text, error });

  function render() {
    if (!state.hasSource) {
      editPeaks = [];
      state.rendered = null;
      waveformVersion++;
      return;
    }
    const tail = (REVERSE_BARS[params.reverse] ?? 1) * 4 * (60 / state.bpm);
    const pre = 0.02; // fixed pre-delay, as in the engine
    const sel = Math.max(0.01, state.selEnd - state.selStart);
    const withOriginal = params.includeOriginal > 0.5;
    const total = withOriginal ? tail + pre + sel : tail;   // the swell stops where the phrase begins
    const decay = 0.18 + (params.size / 150) * 1.7;
    const fadeLen = (params.fade / 100) * tail;
    const rnd = seededNoise(11);

    editPeaks = Array.from({ length: BUCKETS }, (_, i) => {
      const t = (i / BUCKETS) * total;
      let swell: number;
      if (t < tail) swell = Math.exp(-(tail - t) / decay);
      else swell = 0;
      if (fadeLen > 0 && t < fadeLen) swell *= (t / fadeLen) ** 2;
      if (params.tremoloOn > 0.5) swell *= 1 - 0.8 * 0.5 * (1 - Math.cos(2 * Math.PI * (state.bpm / 30) * (t - tail - pre)));
      if (params.phoneOn > 0.5) swell *= 0.8;
      swell *= 0.94 + 0.12 * (rnd() - 0.5) * 0.4;

      let dry = 0;
      if (withOriginal && t >= tail + pre && t < tail + pre + sel) {
        const srcT = state.selStart + (t - tail - pre);
        const idx = Math.min(BUCKETS - 1, Math.floor((srcT / state.sourceSeconds) * BUCKETS));
        dry = rawPeaks[idx] ?? 0;
      }
      return Math.min(1, Math.max(swell * 0.85, dry));
    });

    state.tailSeconds = tail;
    if (state.sourceKind === "capture") {
      // Pretend the take started at bar 9 at 120 BPM (2 s per bar).
      const bar = (sec: number) => {
        const beats = 32 + sec * 2;
        return `bar ${Math.floor(beats / 4) + 1}.${Math.floor(beats % 4) + 1}`;
      };
      state.phraseAt = bar(state.selStart);
      state.swellAt = bar(state.selStart - tail - pre);
      state.sibilantRegions = [[state.selStart + 0.05, state.selStart + 0.13], [state.selStart + sel * 0.62, state.selStart + sel * 0.62 + 0.09]];
    } else {
      state.phraseAt = state.swellAt = "";
      state.sibilantRegions = [];
    }
    state.rendered = {
      seconds: total,
      sampleRate: 48000,
      originalStart: tail + pre,
      originalEnd: tail + pre + sel,
      includesOriginal: withOriginal,
    };
    waveformVersion++;
  }

  const waveforms = (): Waveforms => ({ raw: rawPeaks, edit: editPeaks, version: waveformVersion });

  function publish(withWaveforms = true) {
    emit("glacier:state", { ...state });
    if (withWaveforms) emit("glacier:waveforms", waveforms());
  }

  let renderTimer: number | undefined;
  function scheduleRender() {
    window.clearTimeout(renderTimer);
    renderTimer = window.setTimeout(() => {
      render();
      publish();
    }, 60);
  }

  function applyValues(values: Partial<Params>) {
    params = { ...DEFAULT_PARAMS, ...values };
    emit("glacier:params", { ...params });
  }

  function stopPlayback() {
    window.clearInterval(playTimer);
    playback = { ...playback, playing: false, position: 0 };
    emit("glacier:playback", playback);
  }

  function clearSource() {
    window.clearInterval(captureTimer);
    Object.assign(state, {
      hasSource: false, sourceKind: "none", isDemo: false, fileName: "", sourceSeconds: 0,
      selStart: 0, selEnd: 0, rendered: null, hasTimeline: false, phraseAt: "", swellAt: "",
      sibilantRegions: [], capture: "armed",
    });
    rawPeaks = [];
    editPeaks = [];
    waveformVersion++;
  }

  /** Pretends the user pressed play in the DAW: 3 s of a vocal-like take is captured automatically. */
  function simulateDawPlayback() {
    window.clearInterval(captureTimer);
    const startedAt = performance.now() + 1500;
    captureTimer = window.setInterval(() => {
      if (state.capture !== "armed") return window.clearInterval(captureTimer);
      const t = (performance.now() - startedAt) / 1000;
      if (t < 0) return;
      if (t < 3) {
        // A vocal-ish level: words with short gaps, plus a little flicker.
        const word = (a: number, b: number) => (t > a && t < b ? Math.sin(((t - a) / (b - a)) * Math.PI) : 0);
        const level = (word(0.3, 0.8) + word(0.9, 1.6) * 0.8 + word(1.7, 2.5) * 0.9) * (0.7 + 0.3 * Math.random());
        playback = { ...playback, recording: true, hostPlaying: true, captureSeconds: Math.round(t * 10) / 10, level };
        emit("glacier:playback", playback);
        return;
      }
      window.clearInterval(captureTimer);
      const rnd = seededNoise(5);
      rawPeaks = Array.from({ length: BUCKETS }, (_, i) => {
        const s = (i / BUCKETS) * 6;
        const word = (a: number, b: number) => (s > a && s < b ? Math.sin(((s - a) / (b - a)) * Math.PI) : 0);
        return Math.min(1, (word(1.2, 1.7) + word(1.8, 2.5) * 0.8 + word(2.6, 3.4) * 0.9 + word(4.2, 5.1) * 0.7) * (0.75 + 0.25 * rnd()) + 0.004);
      });
      Object.assign(state, {
        hasSource: true, sourceKind: "capture", isDemo: false, fileName: "Track capture", sourceSeconds: 6,
        sourceRate: 48000, capture: "armed", hasTimeline: true, selStart: 1.19, selEnd: 3.45,
      });
      playback = { ...playback, recording: false, hostPlaying: false, captureSeconds: 0, level: 0 };
      emit("glacier:playback", playback);
      render();
      publish();
    }, 33);
  }

  // Like the plugin in a DAW: start empty and listening, then capture on the first "playback".
  clearSource();
  simulateDawPlayback();

  return {
    kind: "mock",

    ready: async (): Promise<InitialPayload> => ({
      state: { ...state },
      params: { ...params },
      waveforms: waveforms(),
      playback,
    }),

    on(event, listener) {
      if (!listeners.has(event)) listeners.set(event, new Set());
      listeners.get(event)!.add(listener as (p: never) => void);
      return () => listeners.get(event)?.delete(listener as (p: never) => void);
    },

    setParam(id, value) {
      params = { ...params, [id]: value };
      emit("glacier:params", { ...params });
      if (!state.presetModified) {
        state.presetModified = true;
        emit("glacier:state", { ...state });
      }
      scheduleRender();
    },
    beginGesture() {},
    endGesture() {},

    setSelection(start, end) {
      state.selStart = Math.max(0, Math.min(start, end));
      state.selEnd = Math.min(state.sourceSeconds, Math.max(start, end));
      scheduleRender();
    },

    play(source) {
      window.clearInterval(playTimer);
      const length = source === "raw" ? state.selEnd - state.selStart : state.rendered?.seconds ?? 0;
      if (length <= 0) return;
      playStartedAt = performance.now();
      playback = { ...playback, playing: true, source, position: 0 };
      emit("glacier:playback", playback);
      playTimer = window.setInterval(() => {
        let pos = (performance.now() - playStartedAt) / 1000;
        if (pos >= length) {
          if (!state.loop) return stopPlayback();
          playStartedAt = performance.now();
          pos = 0;
        }
        playback = { ...playback, position: pos };
        emit("glacier:playback", playback);
      }, 33);
    },
    stop: stopPlayback,

    setLoop(loop) {
      state.loop = loop;
      publish(false);
    },

    // Clears the take and listens again; the mock then "plays the DAW" to show a fresh capture.
    reset() {
      stopPlayback();
      params = { ...params, lock: 0 };
      emit("glacier:params", { ...params });
      clearSource();
      publish();
      simulateDawPlayback();
    },

    async loadPreset(index) {
      const all = [...FACTORY, ...userPresets];
      if (index < 0 || index >= all.length) return { ok: false, message: "No such preset" };
      applyValues(all[index].values);
      state.presetIndex = index;
      state.presetModified = false;
      render();
      publish();
      return { ok: true, message: "" };
    },

    async savePreset(rawName): Promise<Result> {
      const name = rawName.trim().slice(0, 48);
      if (!name) return { ok: false, message: "Give the preset a name." };
      if (FACTORY.some((p) => p.name.toLowerCase() === name.toLowerCase()))
        return { ok: false, message: `"${name}" is a factory preset. Choose another name.` };
      const existing = userPresets.findIndex((p) => p.name.toLowerCase() === name.toLowerCase());
      if (existing >= 0) userPresets[existing].values = { ...params };
      else userPresets.push({ name, values: { ...params } });
      userPresets.sort((a, b) => a.name.localeCompare(b.name));
      state.presets = [...FACTORY.map((p) => p.name), ...userPresets.map((p) => p.name)];
      state.presetIndex = state.presets.findIndex((p) => p.toLowerCase() === name.toLowerCase());
      state.presetModified = false;
      publish(false);
      return { ok: true, message: "Preset saved" };
    },

    openFile() {
      const input = document.createElement("input");
      input.type = "file";
      input.accept = "audio/*";
      input.onchange = async () => {
        const file = input.files?.[0];
        if (!file) return;
        const r = await this.loadAudioFile(file);
        toast(r.message, !r.ok);
      };
      input.click();
    },
    exportWav: () => toast("Export writes a WAV inside the plugin. This browser preview has no audio engine."),
    openExportsFolder: () => toast(`Exports are saved to ${state.exportsFolder}`),
    setTheme: (theme) => {
      try {
        localStorage.setItem("glacierriser.theme", theme);
      } catch {
        // storage unavailable: the choice lasts for this page only
      }
    },
    startDragOut: async () => ({ ok: false, message: "Drag into your DAW works inside the plugin." }),

    async loadAudioFile(file) {
      try {
        const ctx = new AudioContext();
        const buffer = await ctx.decodeAudioData(await file.arrayBuffer());
        void ctx.close();
        rawPeaks = peaksFromAudioBuffer(buffer);
        state.isDemo = false;
        state.sourceKind = "file";
        state.capture = "off";
        state.hasTimeline = false;
        state.fileName = file.name;
        state.sourceSeconds = buffer.duration;
        state.sourceRate = buffer.sampleRate;
        state.channels = buffer.numberOfChannels;
        state.selStart = 0;
        state.selEnd = buffer.duration > 8 ? 4 : buffer.duration;
        render();
        publish();
        return { ok: true, message: `Loaded ${file.name}` };
      } catch {
        return { ok: false, message: `Could not decode ${file.name}` };
      }
    },
  };
}
