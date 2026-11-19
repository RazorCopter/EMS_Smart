import { FormEvent, useState } from "react";
import { Lock } from "lucide-react";
import { apiFetch, type AuthUser } from "../lib/api";
import { ShadowBanner } from "./ShadowBanner";

interface Props {
  onSuccess: (user: AuthUser) => void;
}

export function LoginScreen({ onSuccess }: Props) {
  const [username, setUsername] = useState("admin");
  const [password, setPassword] = useState("");
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);

  async function handleSubmit(e: FormEvent) {
    e.preventDefault();
    setError(null);
    setLoading(true);
    try {
      const user = await apiFetch<AuthUser>("/api/v1/auth/login", {
        method: "POST",
        body: JSON.stringify({ username, password }),
      });
      onSuccess(user);
    } catch (err) {
      setError(err instanceof Error ? err.message : "Errore di autenticazione");
    } finally {
      setLoading(false);
    }
  }

  return (
    <div
      style={{
        minHeight: "100vh",
        display: "grid",
        placeItems: "center",
        padding: 20,
        background: "var(--bg-canvas)",
      }}
    >
      <div className="card" style={{ width: "min(430px, 100%)", padding: 24 }}>
        <div style={{ display: "flex", alignItems: "center", gap: 12, marginBottom: 14 }}>
          <div className="brand-mark" aria-hidden="true">
            <Lock size={18} color="var(--accent-teal)" />
          </div>
          <div>
            <h1 style={{ fontSize: 19, margin: 0 }}>Afore EMS Companion</h1>
            <p className="page-sub" style={{ margin: 0 }}>
              Accesso locale in sola lettura (Viewer / Admin)
            </p>
          </div>
        </div>

        <ShadowBanner compact />

        <form onSubmit={handleSubmit} style={{ display: "flex", flexDirection: "column", gap: 12, marginTop: 16 }}>
          <label style={{ display: "grid", gap: 6, fontSize: 13, color: "var(--text-secondary)" }}>
            Utente
            <input
              className="input-field"
              value={username}
              onChange={(e) => setUsername(e.target.value)}
              autoComplete="username"
              required
            />
          </label>
          <label style={{ display: "grid", gap: 6, fontSize: 13, color: "var(--text-secondary)" }}>
            Password
            <input
              className="input-field"
              type="password"
              value={password}
              onChange={(e) => setPassword(e.target.value)}
              autoComplete="current-password"
              required
            />
          </label>

          {error && (
            <div role="alert" style={{ color: "var(--fault-red)", fontSize: 13 }}>
              {error}
            </div>
          )}

          <button type="submit" className="btn primary" disabled={loading} style={{ minHeight: 42, marginTop: 4 }}>
            {loading ? "Accesso in corso..." : "Accedi al Companion"}
          </button>
        </form>
      </div>
    </div>
  );
}
