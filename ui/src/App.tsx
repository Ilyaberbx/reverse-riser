import { useEffect, useLayoutEffect, useRef, useState } from "react";
import { backend } from "./bridge";
import { EffectModeSelector } from "./components/EffectModeSelector";
import { BackdropIce, PanelIce } from "./components/IceDecor";
import { PluginHeader } from "./components/PluginHeader";
import { ReverseOrb } from "./components/ReverseOrb";
import { SideKnob } from "./components/SideKnob";
import { ToastHost } from "./components/ToastHost";
import { WaveformDisplay } from "./components/WaveformDisplay";
import { FADE, SIZE } from "./params";
import { actions, showToast, useStore } from "./store";

export const DESIGN_W = 860;
export const DESIGN_H = 680;

/** Uniformly scales the 860 x 680 design to the window (the plugin keeps this aspect ratio). */
function useStageScale() {
  const [fit, setFit] = useState({ scale: 1, left: 0, top: 0 });

  useLayoutEffect(() => {
    const update = () => {
      const scale = Math.min(window.innerWidth / DESIGN_W, window.innerHeight / DESIGN_H);
      setFit({
        scale,
        left: (window.innerWidth - DESIGN_W * scale) / 2,
        top: (window.innerHeight - DESIGN_H * scale) / 2,
      });
    };
    update();
    window.addEventListener("resize", update);
    return () => window.removeEventListener("resize", update);
  }, []);

  return fit;
}

/** Accept audio files dropped anywhere on the plugin. */
function useFileDrop() {
  const [active, setActive] = useState(false);
  const depth = useRef(0);

  useEffect(() => {
    const hasFiles = (e: DragEvent) => Array.from(e.dataTransfer?.types ?? []).includes("Files");

    const onEnter = (e: DragEvent) => {
      if (!hasFiles(e)) return;
      e.preventDefault();
      depth.current++;
      setActive(true);
    };
    const onOver = (e: DragEvent) => {
      if (!hasFiles(e)) return;
      e.preventDefault();
      if (e.dataTransfer) e.dataTransfer.dropEffect = "copy";
    };
    const onLeave = (e: DragEvent) => {
      if (!hasFiles(e)) return;
      depth.current = Math.max(0, depth.current - 1);
      if (depth.current === 0) setActive(false);
    };
    const onDrop = async (e: DragEvent) => {
      if (!hasFiles(e)) return;
      e.preventDefault();
      depth.current = 0;
      setActive(false);
      const file = Array.from(e.dataTransfer?.files ?? [])[0];
      if (!file) return;
      showToast(`Loading ${file.name}…`);
      const r = await backend.loadAudioFile(file);
      showToast(r.message || (r.ok ? `Loaded ${file.name}` : "Could not load that file"), !r.ok);
    };

    window.addEventListener("dragenter", onEnter);
    window.addEventListener("dragover", onOver);
    window.addEventListener("dragleave", onLeave);
    window.addEventListener("drop", onDrop);
    return () => {
      window.removeEventListener("dragenter", onEnter);
      window.removeEventListener("dragover", onOver);
      window.removeEventListener("dragleave", onLeave);
      window.removeEventListener("drop", onDrop);
    };
  }, []);

  return active;
}

function CenterOrb() {
  const reverse = useStore((s) => s.params.reverse);
  const tailSeconds = useStore((s) => s.state.tailSeconds);

  return (
    <ReverseOrb
      value={Math.round(reverse)}
      tailSeconds={tailSeconds}
      onChange={(v) => actions.setParam("reverse", v)}
      onGestureStart={() => actions.beginGesture("reverse")}
      onGestureEnd={() => actions.endGesture("reverse")}
    />
  );
}

export default function App() {
  const { scale, left, top } = useStageScale();
  const dropActive = useFileDrop();
  const connected = useStore((s) => s.connected);

  return (
    <div className="viewport">
      <div
        className={`stage ${connected ? "is-ready" : ""}`}
        style={{ width: DESIGN_W, height: DESIGN_H, transform: `translate(${left}px, ${top}px) scale(${scale})` }}
      >
        <div className="backdrop" />
        <BackdropIce />

        <main className="panel">
          <PanelIce />
          <PluginHeader />
          <div className="panel__waveform">
            <WaveformDisplay dropActive={dropActive} />
          </div>
          <div className="panel__main">
            <SideKnob side="left" param={SIZE} />
            <CenterOrb />
            <SideKnob side="right" param={FADE} />
          </div>
          <div className="panel__footer">
            <EffectModeSelector />
          </div>
          <ToastHost />
        </main>
      </div>
    </div>
  );
}
