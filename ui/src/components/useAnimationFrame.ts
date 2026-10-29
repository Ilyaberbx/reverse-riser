import { useEffect, useRef, useState } from "react";

export function usePrefersReducedMotion(): boolean {
  const [reduced, setReduced] = useState(
    () => typeof window !== "undefined" && window.matchMedia("(prefers-reduced-motion: reduce)").matches,
  );

  useEffect(() => {
    const mq = window.matchMedia("(prefers-reduced-motion: reduce)");
    const onChange = () => setReduced(mq.matches);
    mq.addEventListener("change", onChange);
    return () => mq.removeEventListener("change", onChange);
  }, []);

  return reduced;
}

/**
    Calls `draw(seconds)` every frame from one continuous loop with a clock that never resets,
    so data or parameter changes update what is drawn without restarting the motion.
    With reduced motion it draws a still frame whenever `deps` change.
*/
export function useAnimationFrame(draw: (time: number) => void, animate: boolean, deps: unknown[]) {
  const drawRef = useRef(draw);
  drawRef.current = draw;
  const startRef = useRef<number | null>(null);

  useEffect(() => {
    if (!animate) return;
    if (startRef.current === null) startRef.current = performance.now();
    let frame = 0;
    const tick = (now: number) => {
      drawRef.current((now - (startRef.current ?? now)) / 1000);
      frame = requestAnimationFrame(tick);
    };
    frame = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(frame);
  }, [animate]);

  useEffect(() => {
    if (!animate) drawRef.current(0);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [animate, ...deps]);
}

/** Sets a canvas backing store to match its CSS size times the device pixel ratio. */
export function sizeCanvas(canvas: HTMLCanvasElement, cssWidth: number, cssHeight: number) {
  const rect = canvas.getBoundingClientRect();
  const scale = (rect.width / cssWidth || 1) * (window.devicePixelRatio || 1);
  const w = Math.max(1, Math.round(cssWidth * scale));
  const h = Math.max(1, Math.round(cssHeight * scale));
  if (canvas.width !== w || canvas.height !== h) {
    canvas.width = w;
    canvas.height = h;
  }
  const ctx = canvas.getContext("2d")!;
  ctx.setTransform(scale, 0, 0, scale, 0, 0);
  return ctx;
}
