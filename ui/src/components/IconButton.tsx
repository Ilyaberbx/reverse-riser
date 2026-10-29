import type { ButtonHTMLAttributes, ReactNode } from "react";

/** Square-ish frosted button holding a single icon (preset nav, save, menu). */
export function IconButton({
  label,
  active = false,
  className = "",
  children,
  ...rest
}: ButtonHTMLAttributes<HTMLButtonElement> & { label: string; active?: boolean; children: ReactNode }) {
  return (
    <button
      type="button"
      aria-label={label}
      title={label}
      className={`icon-button ${active ? "is-active" : ""} ${className}`}
      {...rest}
    >
      {children}
    </button>
  );
}

/** Button with icon + label used inside the waveform panel (Listen, Loop, Reset). */
export function PlaybackButton({
  icon,
  label,
  active = false,
  className = "",
  ...rest
}: ButtonHTMLAttributes<HTMLButtonElement> & { icon: ReactNode; label: string; active?: boolean }) {
  return (
    <button type="button" className={`playback-button ${active ? "is-active" : ""} ${className}`} {...rest}>
      <span className="playback-button__icon">{icon}</span>
      <span className="playback-button__label">{label}</span>
    </button>
  );
}

/** Two-or-more segment switch (RAW / EDIT). */
export function SegmentedControl<T extends string>({
  options,
  value,
  onChange,
  ariaLabel,
}: {
  options: { value: T; label: string }[];
  value: T;
  onChange: (v: T) => void;
  ariaLabel: string;
}) {
  return (
    <div className="segmented" role="radiogroup" aria-label={ariaLabel}>
      {options.map((o) => (
        <button
          key={o.value}
          type="button"
          role="radio"
          aria-checked={o.value === value}
          className={`segmented__option ${o.value === value ? "is-active" : ""}`}
          onClick={() => onChange(o.value)}
        >
          {o.label}
        </button>
      ))}
    </div>
  );
}
