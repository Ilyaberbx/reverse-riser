/** Shapes shared by the JUCE backend (C++) and the browser mock. Keep in sync with PluginProcessor.cpp. */

export type ParamId =
  | "reverse"
  | "fade"
  | "size"
  | "phoneOn"
  | "chorusOn"
  | "tremoloOn"
  | "includeOriginal"
  | "phoneMix"
  | "phoneTone"
  | "chorusMix"
  | "chorusRate"
  | "tremoloMix"
  | "tremoloTiming"
  | "level"
  | "lowCut"
  | "lock";

export type Params = Record<ParamId, number>;

export interface RenderedInfo {
  seconds: number;
  sampleRate: number;
  originalStart: number;
  originalEnd: number;
  includesOriginal: boolean;
}

export type SourceKind = "none" | "demo" | "file" | "capture";
export type CaptureState = "off" | "armed" | "recording";

export interface PluginState {
  hasSource: boolean;
  sourceKind: SourceKind;
  isDemo: boolean;
  isStandalone: boolean;
  /** Track capture: every DAW playback records ("recording") and replaces the take. Automatic. */
  capture: CaptureState;
  /** True when the source is a capture that knows where it sits on the timeline. */
  hasTimeline: boolean;
  /** Human-readable timeline positions ("bar 17.1" or "0:31.250"), empty when unknown. */
  phraseAt: string;
  swellAt: string;
  /** Sibilant passages, in source seconds. */
  sibilantRegions: [number, number][];
  fileName: string;
  sourceSeconds: number;
  sourceRate: number;
  channels: number;
  selStart: number;
  selEnd: number;
  rendered: RenderedInfo | null;
  presets: string[];
  factoryPresetCount: number;
  presetIndex: number;
  presetModified: boolean;
  bpm: number;
  tailSeconds: number;
  loop: boolean;
  exportsFolder: string;
}

export interface Waveforms {
  raw: number[];
  edit: number[];
  version: number;
}

export type PlaySource = "raw" | "edit";

export interface Playback {
  playing: boolean;
  source: PlaySource;
  position: number;
  /** DAW transport is running. */
  hostPlaying: boolean;
  recording: boolean;
  /** Input peak (0..1) since the last update, while recording. Drives the live wave. */
  level: number;
  captureSeconds: number;
}

export interface Toast {
  text: string;
  error: boolean;
}

export interface Result {
  ok: boolean;
  message: string;
}

export interface InitialPayload {
  state: PluginState;
  params: Params;
  waveforms: Waveforms;
  playback: Playback;
}

export interface BackendEvents {
  "atmos:state": PluginState;
  "atmos:params": Params;
  "atmos:waveforms": Waveforms;
  "atmos:playback": Playback;
  "atmos:toast": Toast;
}

export type BackendEventName = keyof BackendEvents;

export interface Backend {
  readonly kind: "juce" | "mock";
  ready(): Promise<InitialPayload>;
  on<E extends BackendEventName>(event: E, listener: (payload: BackendEvents[E]) => void): () => void;

  setParam(id: ParamId, value: number): void;
  beginGesture(id: ParamId): void;
  endGesture(id: ParamId): void;

  setSelection(start: number, end: number): void;
  play(source: PlaySource): void;
  stop(): void;
  setLoop(loop: boolean): void;
  /** Clears the captured or loaded audio so the next DAW playback captures again. */
  reset(): void;

  loadPreset(index: number): Promise<Result>;
  savePreset(name: string): Promise<Result>;

  openFile(): void;
  exportWav(): void;
  openExportsFolder(): void;
  startDragOut(): Promise<Result>;
  loadAudioFile(file: File): Promise<Result>;
}
