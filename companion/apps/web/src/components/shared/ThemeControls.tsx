import { Check, Monitor, Moon, RotateCcw, Sun } from "lucide-react";
import { THEME_REGISTRY } from "../../theme/themeRegistry";
import {
  type ColorSchemePreference,
  type ThemeStyle,
  type UiDensity,
} from "../../theme/themeTypes";
import { useTheme } from "../../theme/useTheme";

export function ThemeSelector() {
  const { preferences, runtimeFallbackActive, updatePreferences, clearTemporaryFallback } = useTheme();
  const styles: ThemeStyle[] = ["classic", "emerald"];

  return (
    <div>
      <div style={{ fontSize: 12.5, color: "var(--text-secondary)", marginBottom: 8, fontWeight: 600 }}>
        A. Stile interfaccia
      </div>
      <div className="theme-style-grid" role="radiogroup" aria-label="Seleziona stile interfaccia">
        {styles.map((id) => {
          const meta = THEME_REGISTRY[id];
          const selected = preferences.style === id && !runtimeFallbackActive;
          const p = meta.preview;
          return (
            <button
              key={id}
              type="button"
              role="radio"
              aria-checked={selected}
              className={`theme-style-option ${selected ? "active" : ""}`}
              onClick={() => {
                clearTemporaryFallback();
                updatePreferences({ style: id });
              }}
            >
              <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", gap: 8 }}>
                <strong style={{ fontSize: 14, color: "var(--text-primary)" }}>{meta.name}</strong>
                <span className={`badge ${selected ? "measured" : "unavailable"}`}>
                  {selected ? (
                    <>
                      <Check size={12} aria-hidden="true" /> Attivo
                    </>
                  ) : (
                    meta.statusLabel
                  )}
                </span>
              </div>

              <div
                className="theme-preview-canvas"
                aria-hidden="true"
                style={{ background: p.canvas, border: `1px solid ${p.border}` }}
              >
                <div className="theme-preview-sidebar" style={{ background: p.sidebar, border: `1px solid ${p.border}` }}>
                  <div style={{ height: 5, borderRadius: 2, background: p.accent }} />
                  <div style={{ height: 4, borderRadius: 2, background: p.border, width: "80%" }} />
                  <div style={{ height: 4, borderRadius: 2, background: p.border, width: "65%" }} />
                </div>
                <div className="theme-preview-main">
                  <div className="theme-preview-card" style={{ background: p.surface, border: `1px solid ${p.border}` }}>
                    <div style={{ width: "60%", height: 5, borderRadius: 2, background: p.text, opacity: 0.7 }} />
                    <div className="theme-preview-dots">
                      <span style={{ background: p.pv }} />
                      <span style={{ background: p.battery }} />
                      <span style={{ background: p.grid }} />
                      <span style={{ background: p.home }} />
                    </div>
                  </div>
                  <div className="theme-preview-card" style={{ background: p.surface, border: `1px solid ${p.border}` }}>
                    <div style={{ width: "75%", height: 5, borderRadius: 2, background: p.accent }} />
                    <div style={{ width: "45%", height: 7, borderRadius: 3, background: p.text, opacity: 0.45 }} />
                  </div>
                </div>
              </div>

              <span style={{ fontSize: 12, color: "var(--text-secondary)", lineHeight: 1.35 }}>{meta.tagline}</span>
            </button>
          );
        })}
      </div>
    </div>
  );
}

export function ColorSchemeSelector() {
  const { preferences, resolvedScheme, updatePreferences } = useTheme();
  const options: Array<{ id: ColorSchemePreference; label: string; icon: typeof Moon }> = [
    { id: "dark", label: "Scuro", icon: Moon },
    { id: "light", label: "Chiaro", icon: Sun },
    { id: "system", label: `Sistema (${resolvedScheme === "dark" ? "Scuro" : "Chiaro"})`, icon: Monitor },
  ];

  return (
    <div>
      <div style={{ fontSize: 12.5, color: "var(--text-secondary)", marginBottom: 8, fontWeight: 600 }}>
        B. Schema colore (indipendente dallo stile)
      </div>
      <div className="option-pill-group" role="radiogroup" aria-label="Seleziona schema colore">
        {options.map(({ id, label, icon: Icon }) => {
          const active = preferences.color_scheme === id;
          return (
            <button
              key={id}
              type="button"
              role="radio"
              aria-checked={active}
              className={`option-pill-btn ${active ? "active" : ""}`}
              onClick={() => updatePreferences({ color_scheme: id })}
            >
              <Icon size={14} aria-hidden="true" />
              {label}
            </button>
          );
        })}
      </div>
    </div>
  );
}

export function DensitySelector() {
  const { preferences, updatePreferences } = useTheme();
  const options: Array<{ id: UiDensity; label: string }> = [
    { id: "comfortable", label: "Comoda" },
    { id: "compact", label: "Compatta" },
  ];

  return (
    <div>
      <div style={{ fontSize: 12.5, color: "var(--text-secondary)", marginBottom: 8, fontWeight: 600 }}>
        Densità interfaccia
      </div>
      <div className="option-pill-group" role="radiogroup" aria-label="Seleziona densità interfaccia">
        {options.map(({ id, label }) => {
          const active = preferences.density === id;
          return (
            <button
              key={id}
              type="button"
              role="radio"
              aria-checked={active}
              className={`option-pill-btn ${active ? "active" : ""}`}
              onClick={() => updatePreferences({ density: id })}
            >
              {label}
            </button>
          );
        })}
      </div>
    </div>
  );
}

export function ThemeSettingsCard() {
  const { preferences, syncing, runtimeFallbackActive, updatePreferences, resetToDefault } = useTheme();

  return (
    <article className="card" style={{ gridColumn: "1 / -1" }}>
      <div className="card-header">
        <div>
          <h2 className="card-title">Aspetto &amp; Tema Multi-Style</h2>
          <p className="card-sub">
            Preferenza salvata per il tuo utente · Cambio istantaneo senza ricaricare né interrompere lo stream WebSocket
          </p>
        </div>
        <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
          {syncing && <span className="badge simulated">Sincronizzazione...</span>}
          {runtimeFallbackActive && <span className="badge safety">Fallback Classic attivo</span>}
          <button type="button" className="btn" onClick={resetToDefault}>
            <RotateCcw size={14} aria-hidden="true" />
            Ripristina tema predefinito
          </button>
        </div>
      </div>

      <div style={{ display: "flex", flexDirection: "column", gap: 16 }}>
        <ThemeSelector />
        <div className="grid-2">
          <ColorSchemeSelector />
          <DensitySelector />
        </div>
        <label style={{ display: "inline-flex", alignItems: "center", gap: 10, fontSize: 13, cursor: "pointer" }}>
          <input
            type="checkbox"
            checked={preferences.reduced_motion}
            onChange={(e) => updatePreferences({ reduced_motion: e.target.checked })}
          />
          <span>
            <strong>Riduci animazioni</strong>{" "}
            <span style={{ color: "var(--text-muted)" }}>(ferma particelle Energy Monitor e transizioni rapide)</span>
          </span>
        </label>
      </div>
    </article>
  );
}
