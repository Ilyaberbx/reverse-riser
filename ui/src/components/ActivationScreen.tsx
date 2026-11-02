import { useState } from "react";
import { backend } from "../bridge";
import { useStore } from "../store";

/** Shown instead of the panel until a license key activates this machine. Audio passes through meanwhile. */
export function ActivationScreen() {
  const license = useStore((s) => s.license);
  const [key, setKey] = useState("");

  return (
    <div className="dialog-backdrop">
      <form
        className="dialog"
        role="dialog"
        aria-modal="true"
        aria-labelledby="activate-title"
        onSubmit={(e) => {
          e.preventDefault();
          if (key.trim()) backend.activate(key);
        }}
      >
        <h2 id="activate-title">Activate GlacierRiser</h2>
        <p className="dialog__hint">
          Paste the license key from your purchase email or your account. One key activates up to 2 machines.
        </p>
        <input
          className="dialog__input"
          value={key}
          onChange={(e) => setKey(e.target.value)}
          placeholder="XXXX-XXXX-XXXX-XXXX"
          spellCheck={false}
          autoFocus
          aria-label="License key"
        />
        {license.error && <p className="dialog__error">{license.error}</p>}
        <div className="dialog__actions">
          <button type="button" className="text-button" onClick={() => backend.openAccount()}>
            Find my key
          </button>
          <button type="submit" className="text-button is-primary" disabled={license.busy || !key.trim()}>
            {license.busy ? "Activating…" : "Activate"}
          </button>
        </div>
      </form>
    </div>
  );
}
