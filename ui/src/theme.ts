/**
    UI theme (light "Frost" / dark "Night Ice"). Kept apart from the parameter store on purpose:
    it is a per-user look, saved by the host side for every instance, and never touches presets,
    the Edited flag, undo or the DAW project.
*/

import { useSyncExternalStore } from "react";
import { backend, type Theme } from "./bridge";

const isTheme = (v: unknown): v is Theme => v === "light" || v === "dark";

function initialTheme(): Theme {
  // Inside the plugin the editor injects the saved theme before the page loads (no flash).
  const injected = window.__JUCE__?.initialisationData?.theme?.[0];
  if (isTheme(injected)) return injected;
  try {
    const stored = localStorage.getItem("glacierriser.theme");
    if (isTheme(stored)) return stored;
  } catch {
    // storage unavailable
  }
  return "light";
}

let current: Theme = initialTheme();
const listeners = new Set<() => void>();

function apply(theme: Theme, animate: boolean) {
  if (theme === current && document.documentElement.dataset.theme === theme) return;
  current = theme;
  const swap = () => {
    document.documentElement.dataset.theme = theme;
    listeners.forEach((fn) => fn());
  };
  const reduced = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  if (animate && !reduced && document.startViewTransition) document.startViewTransition(swap);
  else swap();
}

/** Sets the theme on the page before the first render, and follows other instances afterwards. */
export function initTheme() {
  document.documentElement.dataset.theme = current;
  backend.on("glacier:theme", (theme) => {
    if (isTheme(theme)) apply(theme, true);
  });
}

export function toggleTheme() {
  const next: Theme = current === "dark" ? "light" : "dark";
  apply(next, true);
  backend.setTheme(next);
}

export function useTheme(): Theme {
  return useSyncExternalStore(
    (fn) => {
      listeners.add(fn);
      return () => listeners.delete(fn);
    },
    () => current,
  );
}
