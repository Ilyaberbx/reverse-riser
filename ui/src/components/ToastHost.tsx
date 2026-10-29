import { useEffect, useState } from "react";
import { useStore } from "../store";

/** Short status line under the header (loads, saves, errors). */
export function ToastHost() {
  const toast = useStore((s) => s.toast);
  const [visibleId, setVisibleId] = useState<number | null>(null);

  useEffect(() => {
    if (!toast) return;
    setVisibleId(toast.id);
    const timer = window.setTimeout(() => setVisibleId(null), toast.error ? 6000 : 3200);
    return () => window.clearTimeout(timer);
  }, [toast]);

  if (!toast || visibleId !== toast.id) return null;

  return (
    <div className={`toast ${toast.error ? "is-error" : ""}`} role="status" aria-live="polite">
      {toast.text}
    </div>
  );
}
