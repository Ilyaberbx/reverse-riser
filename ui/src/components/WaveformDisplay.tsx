import { useEffect, useMemo, useRef, useState } from "react";
import type { PointerEvent } from "react";
import { backend } from "../bridge";
import { actions, getStore, showToast, useStore } from "../store";
import { LockIcon, LoopIcon, PlayIcon, ResetIcon, StopIcon } from "./icons";
import { PlaybackButton, SegmentedControl } from "./IconButton";
import { sizeCanvas, useAnimationFrame, usePrefersReducedMotion } from "./useAnimationFrame";
import {
  drawAtmosphere,
  drawBarLayer,
  drawLiveWave,
  drawPlayhead,
  LIVE_BARS,
  fractionForX,
  WAVE_H,
  WAVE_W,
  xForFraction,
} from "./waveformRenderer";

type View = "raw" | "edit";

const LIVE_STEP_MS = 1000 / 30; // one live bar per 33 ms (about 5 s across the panel)
type DragKind = "create" | "start" | "end";

const HANDLE_GRAB = 8;
const MIN_SELECTION = 0.01;

export function WaveformDisplay({ dropActive }: { dropActive: boolean }) {
  const [view, setView] = useState<View>("edit");
  const waveforms = useStore((s) => s.waveforms);
  const state = useStore((s) => s.state);
  const playback = useStore((s) => s.playback);
  const reducedMotion = usePrefersReducedMotion();

  const atmosphereRef = useRef<HTMLCanvasElement>(null);
  const barsRef = useRef<HTMLCanvasElement>(null);
  const overlayRef = useRef<HTMLCanvasElement>(null);

  // Local selection while dragging in RAW view (committed to the backend on release).
  const [localSel, setLocalSel] = useState<[number, number] | null>(null);
  const selDrag = useRef<{ kind: DragKind; anchor: number } | null>(null);
  const dragOut = useRef<{ x: number; y: number; started: boolean } | null>(null);
  const liveRef = useRef({ active: false, levels: [] as number[], pending: 0, lastSample: 0 });

  const sourceSeconds = state.sourceSeconds;
  const rendered = state.rendered;
  const selFractions: [number, number] | null =
    localSel ?? (sourceSeconds > 0 ? [state.selStart / sourceSeconds, state.selEnd / sourceSeconds] : null);

  const peaks = view === "edit" ? waveforms.edit : waveforms.raw;
  const originalFraction =
    rendered && rendered.includesOriginal && rendered.seconds > 0 ? rendered.originalStart / rendered.seconds : null;

  // Energy profile (0..1, ~120 points) that steers the liquid lines.
  const emphasis = useMemo(() => {
    if (peaks.length === 0) return [];
    const n = 120;
    const max = Math.max(1e-4, ...peaks);
    return Array.from({ length: n }, (_, i) => {
      const a = Math.floor((i / n) * peaks.length);
      const b = Math.max(a + 1, Math.floor(((i + 1) / n) * peaks.length));
      let m = 0;
      for (let k = a; k < b; k++) m = Math.max(m, peaks[k]);
      return m / max;
    });
  }, [peaks]);

  // The cached layer is drawn at screen resolution, so redraw it when the window is resized.
  const [resizeTick, setResizeTick] = useState(0);
  useEffect(() => {
    const onResize = () => setResizeTick((n) => n + 1);
    window.addEventListener("resize", onResize);
    return () => window.removeEventListener("resize", onResize);
  }, []);

  // Cached bar layer.
  useEffect(() => {
    const canvas = barsRef.current;
    if (!canvas) return;
    drawBarLayer(sizeCanvas(canvas, WAVE_W, WAVE_H), {
      view,
      peaks,
      originalFraction: view === "edit" ? originalFraction : null,
      selection: view === "raw" ? selFractions : null,
    });
  }, [view, peaks, originalFraction, selFractions?.[0], selFractions?.[1], resizeTick]); // eslint-disable-line react-hooks/exhaustive-deps

  // Atmosphere (animated) and playhead.
  useAnimationFrame(
    (t) => {
      const atmo = atmosphereRef.current;
      const overlay = overlayRef.current;
      if (!atmo || !overlay) return;
      drawAtmosphere(sizeCanvas(atmo, WAVE_W, WAVE_H), reducedMotion ? 0 : t, emphasis);

      const ctx = sizeCanvas(overlay, WAVE_W, WAVE_H);
      ctx.clearRect(0, 0, WAVE_W, WAVE_H);
      const pb = getStore().playback;
      const st = getStore().state;
      const live = liveRef.current;

      // Recording: sample the input level at a fixed rate so the wave scrolls at a steady speed.
      if (pb.recording) {
        const now = performance.now();
        if (!live.active) {
          live.active = true;
          live.levels = [];
          live.lastSample = now;
        }
        while (now - live.lastSample >= LIVE_STEP_MS) {
          live.levels.push(live.pending);
          live.pending = 0;
          live.lastSample += LIVE_STEP_MS;
          if (live.levels.length > LIVE_BARS) live.levels.shift();
        }
        drawLiveWave(ctx, live.levels, (now - live.lastSample) / LIVE_STEP_MS, t);
        return;
      }
      live.active = false;

      if (view === "edit" && st.rendered) {
        const preview = pb.playing && pb.source === "edit";
        drawPlayhead(ctx, preview ? Math.min(1, pb.position / st.rendered.seconds) : 0, preview);
      } else if (view === "raw" && pb.playing && pb.source === "raw" && st.sourceSeconds > 0) {
        drawPlayhead(ctx, Math.min(1, (st.selStart + pb.position) / st.sourceSeconds), true);
      }
    },
    true,
    [view, emphasis, reducedMotion],
  );

  // Collect the loudest input level between animation samples.
  useEffect(() => {
    if (playback.recording) liveRef.current.pending = Math.max(liveRef.current.pending, playback.level);
  }, [playback]);

  //------------------------------------------------------------------------------
  const toDesignX = (e: PointerEvent<HTMLCanvasElement>) => {
    const rect = e.currentTarget.getBoundingClientRect();
    return ((e.clientX - rect.left) / rect.width) * WAVE_W;
  };

  const onPointerDown = (e: PointerEvent<HTMLCanvasElement>) => {
    if (e.button !== 0 || !state.hasSource) return;
    const x = toDesignX(e);

    if (view === "edit") {
      if (rendered) dragOut.current = { x: e.clientX, y: e.clientY, started: false };
      return;
    }

    e.currentTarget.setPointerCapture(e.pointerId);
    const f = fractionForX(x);
    const [s0, s1] = selFractions ?? [0, 0];

    if (selFractions && Math.abs(x - xForFraction(s0)) <= HANDLE_GRAB) selDrag.current = { kind: "start", anchor: s1 };
    else if (selFractions && Math.abs(x - xForFraction(s1)) <= HANDLE_GRAB) selDrag.current = { kind: "end", anchor: s0 };
    else {
      selDrag.current = { kind: "create", anchor: f };
      setLocalSel([f, f]);
    }
  };

  const onPointerMove = (e: PointerEvent<HTMLCanvasElement>) => {
    const x = toDesignX(e);

    if (view === "edit") {
      const d = dragOut.current;
      e.currentTarget.style.cursor = rendered ? "grab" : "default";
      if (d && !d.started && (e.buttons & 1) && Math.hypot(e.clientX - d.x, e.clientY - d.y) > 6) {
        d.started = true;
        e.currentTarget.style.cursor = "grabbing";
        void backend.startDragOut().then((r) => {
          dragOut.current = null;
          if (!r.ok || backend.kind === "mock") showToast(r.message, !r.ok && backend.kind === "juce");
        });
      }
      return;
    }

    const drag = selDrag.current;
    if (!drag) {
      const near = selFractions && Math.min(Math.abs(x - xForFraction(selFractions[0])), Math.abs(x - xForFraction(selFractions[1]))) <= HANDLE_GRAB;
      e.currentTarget.style.cursor = near ? "ew-resize" : "text";
      return;
    }

    const f = fractionForX(x);
    setLocalSel([Math.min(drag.anchor, f), Math.max(drag.anchor, f)]);
  };

  const onPointerUp = (e: PointerEvent<HTMLCanvasElement>) => {
    dragOut.current = null;
    if (view !== "raw" || !selDrag.current) return;
    selDrag.current = null;
    if (e.currentTarget.hasPointerCapture(e.pointerId)) e.currentTarget.releasePointerCapture(e.pointerId);

    const sel = localSel;
    setLocalSel(null);
    if (!sel || sourceSeconds <= 0) return;
    if ((sel[1] - sel[0]) * sourceSeconds < MIN_SELECTION) return;   // a click keeps the old selection
    actions.setSelection(sel[0] * sourceSeconds, sel[1] * sourceSeconds);
  };

  const onDoubleClick = () => {
    if (view === "raw" && sourceSeconds > 0) actions.setSelection(0, sourceSeconds);
  };

  //------------------------------------------------------------------------------
  const playingThisView = playback.playing && playback.source === view;

  const toggleListen = () => {
    if (getStore().playback.playing) backend.stop();
    else backend.play(view);
  };

  // Space toggles the preview unless a text field or a focused control wants the key.
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.code !== "Space" || e.repeat) return;
      const t = e.target as HTMLElement | null;
      if (t && (t.tagName === "INPUT" || t.tagName === "BUTTON" || t.getAttribute("role") === "slider")) return;
      e.preventDefault();
      if (getStore().playback.playing) backend.stop();
      else if (getStore().state.hasSource) backend.play(view);
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [view]);

  const recording = playback.recording;
  const locked = useStore((st) => st.params.lock > 0.5);

  return (
    <section className={`waveform ${dropActive ? "is-drop-target" : ""} ${recording ? "is-recording" : ""} ${locked ? "is-locked" : ""}`} aria-label="Waveform">
      <canvas ref={atmosphereRef} className="waveform__layer" style={{ width: WAVE_W, height: WAVE_H }} aria-hidden="true" />
      <canvas ref={barsRef} className="waveform__layer waveform__bars" style={{ width: WAVE_W, height: WAVE_H }} aria-hidden="true" />
      <canvas
        ref={overlayRef}
        className="waveform__layer waveform__interactive"
        style={{ width: WAVE_W, height: WAVE_H }}
        onPointerDown={onPointerDown}
        onPointerMove={onPointerMove}
        onPointerUp={onPointerUp}
        onPointerCancel={onPointerUp}
        onDoubleClick={onDoubleClick}
        aria-label={view === "raw" ? "Source waveform. Drag to select a segment." : "Processed waveform. Drag into your DAW."}
      />

      {dropActive && (
        <div className="waveform__drop" aria-hidden="true" />
      )}

      <div className="waveform__top">
        <button
          type="button"
          className={`waveform__lock ${locked ? "is-on" : ""}`}
          aria-pressed={locked}
          onClick={() => {
            actions.beginGesture("lock");
            actions.setParam("lock", locked ? 0 : 1);
            actions.endGesture("lock");
          }}
          title={
            locked
              ? "Locked: playing the DAW keeps this take. Click to let playback capture again."
              : "Lock this take so playing the DAW doesn't replace it"
          }
        >
          <LockIcon locked={locked} size={14} />
          <span>LOCK</span>
        </button>
        <button
          type="button"
          className="waveform__reset"
          disabled={!state.hasSource && !recording}
          onClick={() => backend.reset()}
          title={
            state.isStandalone
              ? "Clear the audio inside the plugin"
              : "Clear the recording inside the plugin and unlock. The next playback captures the track again."
          }
        >
          <ResetIcon size={14} />
          <span>RESET</span>
        </button>
      </div>

      <div className="waveform__transport">
        <PlaybackButton
          icon={playingThisView ? <StopIcon size={12} /> : <PlayIcon size={12} />}
          label={playingThisView ? "STOP" : "LISTEN"}
          active={playingThisView}
          disabled={!state.hasSource}
          onClick={toggleListen}
          title={view === "raw" ? "Play the selected source segment (space)" : "Play the processed clip (space)"}
        />
        <PlaybackButton
          icon={<LoopIcon size={13} />}
          label="LOOP"
          active={state.loop}
          aria-pressed={state.loop}
          onClick={() => actions.setLoop(!state.loop)}
          title="Repeat the preview"
        />
      </div>

      <div className="waveform__view-switch">
        <SegmentedControl<View>
          ariaLabel="Waveform view"
          value={view}
          onChange={setView}
          options={[
            { value: "raw", label: "RAW" },
            { value: "edit", label: "EDIT" },
          ]}
        />
      </div>
    </section>
  );
}
