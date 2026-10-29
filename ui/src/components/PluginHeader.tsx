import { useEffect, useRef, useState } from "react";
import { backend } from "../bridge";
import { actions, showToast, useStore } from "../store";
import {
  BrandLogo,
  CheckIcon,
  ChevronDown,
  ChevronLeft,
  ChevronRight,
  DownloadIcon,
  FolderIcon,
  MenuIcon,
  MoonIcon,
  OpenFileIcon,
  SavePresetIcon,
  SunIcon,
} from "./icons";
import { IconButton } from "./IconButton";
import { toggleTheme, useTheme } from "../theme";

/** Closes a popover when clicking outside it or pressing Escape. */
function useDismiss(open: boolean, close: () => void) {
  const ref = useRef<HTMLDivElement>(null);
  useEffect(() => {
    if (!open) return;
    const onDown = (e: PointerEvent) => {
      if (ref.current && !ref.current.contains(e.target as Node)) close();
    };
    const onKey = (e: KeyboardEvent) => e.key === "Escape" && close();
    window.addEventListener("pointerdown", onDown);
    window.addEventListener("keydown", onKey);
    return () => {
      window.removeEventListener("pointerdown", onDown);
      window.removeEventListener("keydown", onKey);
    };
  }, [open, close]);
  return ref;
}

async function loadPreset(index: number) {
  const r = await backend.loadPreset(index);
  if (!r.ok) showToast(r.message || "Could not load that preset", true);
}

function PresetSelector() {
  const state = useStore((s) => s.state);
  const [open, setOpen] = useState(false);
  const ref = useDismiss(open, () => setOpen(false));
  const { presets, presetIndex, factoryPresetCount, presetModified } = state;
  const name = presets[presetIndex] ?? "Default";

  const step = (delta: number) => {
    if (presets.length === 0) return;
    void loadPreset((presetIndex + delta + presets.length) % presets.length);
  };

  return (
    <div className="preset" ref={ref}>
      <div className="preset__field">
        <button
          type="button"
          className="preset__name"
          aria-haspopup="listbox"
          aria-expanded={open}
          onClick={() => setOpen((o) => !o)}
        >
          <span className="preset__text">{name}</span>
          {presetModified && <span className="preset__modified" title="Edited since the preset was loaded">edited</span>}
          <ChevronDown size={16} className="preset__caret" />
        </button>
        <button type="button" className="preset__nav" aria-label="Previous preset" title="Previous preset" onClick={() => step(-1)}>
          <ChevronLeft size={20} />
        </button>
        <button type="button" className="preset__nav" aria-label="Next preset" title="Next preset" onClick={() => step(1)}>
          <ChevronRight size={20} />
        </button>
      </div>

      {open && (
        <div className="popover preset__list" role="listbox" aria-label="Presets">
          <div className="popover__heading">Factory</div>
          {presets.map((p, i) => (
            <div key={`${p}-${i}`}>
              {i === factoryPresetCount && <div className="popover__heading">Your presets</div>}
              <button
                type="button"
                role="option"
                aria-selected={i === presetIndex}
                className={`popover__item ${i === presetIndex ? "is-current" : ""}`}
                onClick={() => {
                  setOpen(false);
                  void loadPreset(i);
                }}
              >
                <span>{p}</span>
                {i === presetIndex && <CheckIcon size={15} />}
              </button>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}

function SavePresetDialog({ onClose }: { onClose: () => void }) {
  const current = useStore((s) => s.state.presets[s.state.presetIndex] ?? "");
  const factoryCount = useStore((s) => s.state.factoryPresetCount);
  const presetIndex = useStore((s) => s.state.presetIndex);
  const [name, setName] = useState(presetIndex >= factoryCount ? current : "");
  const [error, setError] = useState("");
  const inputRef = useRef<HTMLInputElement>(null);

  useEffect(() => inputRef.current?.focus(), []);

  const save = async () => {
    const r = await backend.savePreset(name);
    if (r.ok) {
      showToast(`Saved preset "${name.trim()}"`);
      onClose();
    } else setError(r.message);
  };

  return (
    <div className="dialog-backdrop" onPointerDown={(e) => e.target === e.currentTarget && onClose()}>
      <div className="dialog" role="dialog" aria-modal="true" aria-labelledby="save-preset-title">
        <h2 id="save-preset-title">Save preset</h2>
        <p className="dialog__hint">Stores the current knob, orb and mode settings. Saving with an existing name replaces it.</p>
        <input
          ref={inputRef}
          className="dialog__input"
          value={name}
          maxLength={48}
          placeholder="Preset name"
          onChange={(e) => {
            setName(e.target.value);
            setError("");
          }}
          onKeyDown={(e) => {
            if (e.key === "Enter") void save();
            if (e.key === "Escape") onClose();
          }}
        />
        {error && <p className="dialog__error">{error}</p>}
        <div className="dialog__actions">
          <button type="button" className="text-button" onClick={onClose}>
            Cancel
          </button>
          <button type="button" className="text-button is-primary" disabled={!name.trim()} onClick={() => void save()}>
            Save
          </button>
        </div>
      </div>
    </div>
  );
}

function PluginMenu() {
  const [open, setOpen] = useState(false);
  const ref = useDismiss(open, () => setOpen(false));
  const includeOriginal = useStore((s) => s.params.includeOriginal > 0.5);
  const bpm = useStore((s) => s.state.bpm);
  const tail = useStore((s) => s.state.tailSeconds);

  const run = (fn: () => void) => () => {
    setOpen(false);
    fn();
  };

  return (
    <div className="menu" ref={ref}>
      <IconButton label="Menu" active={open} aria-haspopup="menu" aria-expanded={open} onClick={() => setOpen((o) => !o)}>
        <MenuIcon size={26} strokeWidth={2.2} />
      </IconButton>
      {open && (
        <div className="popover menu__list" role="menu">
          <button type="button" role="menuitem" className="popover__item" onClick={run(() => backend.openFile())}>
            <OpenFileIcon size={16} /> <span>Load audio file…</span>
          </button>
          <button type="button" role="menuitem" className="popover__item" onClick={run(() => backend.exportWav())}>
            <DownloadIcon size={16} /> <span>Export WAV…</span>
          </button>
          <button type="button" role="menuitem" className="popover__item" onClick={run(() => backend.openExportsFolder())}>
            <FolderIcon size={16} /> <span>Open exports folder</span>
          </button>
          <div className="popover__divider" />
          <button
            type="button"
            role="menuitemcheckbox"
            aria-checked={includeOriginal}
            className="popover__item"
            onClick={() => {
              actions.beginGesture("includeOriginal");
              actions.setParam("includeOriginal", includeOriginal ? 0 : 1);
              actions.endGesture("includeOriginal");
            }}
          >
            <span className={`checkbox ${includeOriginal ? "is-checked" : ""}`}>{includeOriginal && <CheckIcon size={12} strokeWidth={3} />}</span>
            <span>Include original after the swell</span>
          </button>
          <div className="popover__divider" />
          <div className="popover__footnote">
            Swell {tail.toFixed(2)} s at {Math.round(bpm)} BPM. Tempo follows the host.
          </div>
        </div>
      )}
    </div>
  );
}

/** Sun/moon switch between the light and dark looks. A UI preference only: no parameter changes. */
function ThemeToggle() {
  const dark = useTheme() === "dark";
  const label = dark ? "Light theme" : "Dark theme";
  return (
    <div className="header__ghost">
      <IconButton label={label} onClick={toggleTheme}>
        {dark ? <SunIcon size={21} strokeWidth={2} /> : <MoonIcon size={20} strokeWidth={2} />}
      </IconButton>
    </div>
  );
}

export function PluginHeader() {
  const [saving, setSaving] = useState(false);

  return (
    <header className="header">
      <div className="header__brand">
        <BrandLogo size={40} />
        <span className="wordmark">
          <span className="wordmark__light">GLACIER</span>
          <span className="wordmark__bold">RISER</span>
        </span>
      </div>

      <div className="header__center">
        <PresetSelector />
        <IconButton label="Save preset" className="header__save" onClick={() => setSaving(true)}>
          <SavePresetIcon size={24} />
        </IconButton>
      </div>

      <div className="header__right">
        <ThemeToggle />
        <PluginMenu />
      </div>

      {saving && <SavePresetDialog onClose={() => setSaving(false)} />}
    </header>
  );
}
