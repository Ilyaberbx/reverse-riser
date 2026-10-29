import type { Backend, BackendEventName, BackendEvents, InitialPayload, ParamId, PlaySource, Result } from "./types";

/** The object JUCE's WebBrowserComponent injects when native integration is enabled. */
interface JuceBackendObject {
  addEventListener(eventId: string, fn: (payload: unknown) => void): unknown;
  removeEventListener(token: unknown): void;
  emitEvent(eventId: string, payload: unknown): void;
}

interface JuceGlobal {
  backend: JuceBackendObject;
  initialisationData?: { __juce__functions?: string[]; theme?: string[] };
}

declare global {
  interface Window {
    __JUCE__?: JuceGlobal;
  }
}

export function isRunningInJuce(): boolean {
  const juce = window.__JUCE__;
  return !!juce?.backend && !!juce.initialisationData?.__juce__functions?.includes("ready");
}

/** Talks to GlacierRiserEditor (PluginEditor.cpp) through JUCE's native-function protocol. */
export function createJuceBackend(): Backend {
  const juce = window.__JUCE__!.backend;
  const pending = new Map<number, (result: unknown) => void>();
  let nextId = 0;

  juce.addEventListener("__juce__complete", (payload) => {
    const { promiseId, result } = payload as { promiseId: number; result: unknown };
    pending.get(promiseId)?.(result);
    pending.delete(promiseId);
  });

  function call<T = unknown>(name: string, ...params: unknown[]): Promise<T> {
    const resultId = nextId++;
    return new Promise<T>((resolve) => {
      pending.set(resultId, (r) => resolve(r as T));
      juce.emitEvent("__juce__invoke", { name, params, resultId });
    });
  }

  const asResult = (r: unknown): Result =>
    r && typeof r === "object" ? (r as Result) : { ok: !!r, message: "" };

  async function fileToBase64(file: File): Promise<string> {
    const bytes = new Uint8Array(await file.arrayBuffer());
    let binary = "";
    const chunk = 0x8000;
    for (let i = 0; i < bytes.length; i += chunk)
      binary += String.fromCharCode(...bytes.subarray(i, i + chunk));
    return btoa(binary);
  }

  return {
    kind: "juce",
    ready: () => call<InitialPayload>("ready"),

    on<E extends BackendEventName>(event: E, listener: (payload: BackendEvents[E]) => void) {
      const token = juce.addEventListener(event, (p) => listener(p as BackendEvents[E]));
      return () => juce.removeEventListener(token);
    },

    setParam: (id: ParamId, value: number) => void call("setParam", id, value),
    beginGesture: (id: ParamId) => void call("beginGesture", id),
    endGesture: (id: ParamId) => void call("endGesture", id),

    setSelection: (start, end) => void call("setSelection", start, end),
    play: (source: PlaySource) => void call("play", source),
    stop: () => void call("stop"),
    setLoop: (loop) => void call("setLoop", loop),
    reset: () => void call("reset"),

    loadPreset: async (index) => asResult(await call("loadPreset", index)),
    savePreset: async (name) => asResult(await call("savePreset", name)),

    openFile: () => void call("openFile"),
    exportWav: () => void call("exportWav"),
    openExportsFolder: () => void call("openExportsFolder"),
    setTheme: (theme) => void call("setTheme", theme),
    startDragOut: async () => asResult(await call("startDragOut")),

    loadAudioFile: async (file) => {
      if (file.size > 200 * 1024 * 1024)
        return { ok: false, message: "That file is too large to drop (200 MB max). Use Load audio file instead." };
      return asResult(await call("loadAudioData", file.name, await fileToBase64(file)));
    },
  };
}
