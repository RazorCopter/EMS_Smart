import { useEffect, useState } from "react";
import { BrowserRouter, Route, Routes } from "react-router-dom";
import { useQuery, useQueryClient } from "@tanstack/react-query";
import { apiFetch, type AuthUser } from "./lib/api";
import { Layout } from "./components/Layout";
import { LoginScreen } from "./components/LoginScreen";
import { DashboardPage } from "./pages/DashboardPage";
import { HistoryPage } from "./pages/HistoryPage";
import { DecisionsPage } from "./pages/DecisionsPage";
import { AnalysisPage } from "./pages/AnalysisPage";
import { ComparisonPage } from "./pages/ComparisonPage";
import { BatteryPage } from "./pages/BatteryPage";
import { ReportPage } from "./pages/ReportPage";
import { SettingsPage } from "./pages/SettingsPage";

export function App() {
  const qc = useQueryClient();
  const [theme, setTheme] = useState<"dark" | "light">(() => {
    const saved = localStorage.getItem("afore_companion_theme");
    return saved === "light" ? "light" : "dark";
  });

  useEffect(() => {
    document.documentElement.dataset.theme = theme;
    localStorage.setItem("afore_companion_theme", theme);
  }, [theme]);

  const authQuery = useQuery({
    queryKey: ["auth-me"],
    queryFn: () => apiFetch<AuthUser & { authenticated?: boolean }>("/api/v1/auth/me"),
    retry: false,
  });

  const activeUser: AuthUser | null =
    authQuery.data && authQuery.data.authenticated !== false && authQuery.data.username ? authQuery.data : null;

  const systemQuery = useQuery({
    queryKey: ["system-status"],
    queryFn: () => apiFetch<any>("/api/v1/system/status"),
    enabled: Boolean(activeUser),
    refetchInterval: 10000,
  });

  // Live WebSocket stream for <2s updates on telemetry/decisions
  useEffect(() => {
    if (!activeUser) return;
    const proto = window.location.protocol === "https:" ? "wss:" : "ws:";
    const ws = new WebSocket(`${proto}//${window.location.host}/api/v1/ws/live`);
    ws.onmessage = (evt) => {
      try {
        const msg = JSON.parse(evt.data);
        if (msg.type === "snapshot" || msg.type === "update") {
          qc.setQueryData(["live"], msg.data);
        }
      } catch {
        // ignore keepalive parse issues
      }
    };
    return () => {
      ws.close();
    };
  }, [activeUser, qc]);

  async function handleLogout() {
    await apiFetch("/api/v1/auth/logout", { method: "POST" });
    qc.setQueryData(["auth-me"], { authenticated: false, username: null, role: null, csrf_token: null });
  }

  if (authQuery.isLoading) {
    return <div style={{ padding: 32 }}>Inizializzazione Afore EMS Companion...</div>;
  }

  if (!activeUser) {
    return (
      <LoginScreen
        onSuccess={(user) => {
          qc.setQueryData(["auth-me"], { ...user, authenticated: true });
        }}
      />
    );
  }

  return (
    <BrowserRouter>
      <Routes>
        <Route
          element={
            <Layout
              user={activeUser}
              system={systemQuery.data ?? null}
              theme={theme}
              onToggleTheme={() => setTheme((t) => (t === "dark" ? "light" : "dark"))}
              onLogout={handleLogout}
            />
          }
        >
          <Route index element={<DashboardPage />} />
          <Route path="/storico" element={<HistoryPage />} />
          <Route path="/decisioni" element={<DecisionsPage />} />
          <Route path="/analisi" element={<AnalysisPage />} />
          <Route path="/confronto-sem" element={<ComparisonPage />} />
          <Route path="/batteria" element={<BatteryPage />} />
          <Route path="/report" element={<ReportPage />} />
          <Route
            path="/impostazioni"
            element={<SettingsPage user={activeUser} theme={theme} onChangeTheme={setTheme} />}
          />
        </Route>
      </Routes>
    </BrowserRouter>
  );
}

export default App;
