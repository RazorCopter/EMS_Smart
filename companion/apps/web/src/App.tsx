import { lazy, Suspense, useEffect } from "react";
import { BrowserRouter, Route, Routes } from "react-router-dom";
import { useQuery, useQueryClient } from "@tanstack/react-query";
import { MotionConfig } from "motion/react";
import { apiFetch, type AuthUser } from "./lib/api";
import { ThemeProvider } from "./theme/ThemeProvider";
import { useTheme } from "./theme/useTheme";
import { ThemeErrorBoundary } from "./components/shared/ThemeErrorBoundary";
import { Layout } from "./components/Layout";
import { LoginScreen } from "./components/LoginScreen";
import { DashboardPage } from "./pages/DashboardPage";

const HistoryPage = lazy(() => import("./pages/HistoryPage").then((m) => ({ default: m.HistoryPage })));
const DecisionsPage = lazy(() => import("./pages/DecisionsPage").then((m) => ({ default: m.DecisionsPage })));
const AnalysisPage = lazy(() => import("./pages/AnalysisPage").then((m) => ({ default: m.AnalysisPage })));
const ComparisonPage = lazy(() => import("./pages/ComparisonPage").then((m) => ({ default: m.ComparisonPage })));
const BatteryPage = lazy(() => import("./pages/BatteryPage").then((m) => ({ default: m.BatteryPage })));
const ReportPage = lazy(() => import("./pages/ReportPage").then((m) => ({ default: m.ReportPage })));
const NotificationsPage = lazy(() => import("./pages/NotificationsPage").then((m) => ({ default: m.NotificationsPage })));
const SettingsPage = lazy(() => import("./pages/SettingsPage").then((m) => ({ default: m.SettingsPage })));

function ThemedShell({
  activeUser,
  onLoginSuccess,
  onLogout,
}: {
  activeUser: AuthUser | null;
  onLoginSuccess: (user: AuthUser) => void;
  onLogout: () => void;
}) {
  const { effectiveStyle, activateTemporaryClassicFallback, resetToDefault } = useTheme();

  const systemQuery = useQuery({
    queryKey: ["system-status"],
    queryFn: () => apiFetch<any>("/api/v1/system/status"),
    enabled: Boolean(activeUser),
    refetchInterval: 10000,
  });

  if (!activeUser) {
    return <LoginScreen onSuccess={onLoginSuccess} />;
  }

  const pageFallback = <div className="card">Caricamento sezione...</div>;

  return (
    <MotionConfig reducedMotion="user">
      <ThemeErrorBoundary
        styleName={effectiveStyle}
        onFallbackToClassic={activateTemporaryClassicFallback}
        onResetDefault={resetToDefault}
      >
      <BrowserRouter>
        <Routes>
          <Route
            element={
              <Layout
                user={activeUser}
                system={systemQuery.data ?? null}
                onLogout={onLogout}
              />
            }
          >
            <Route index element={<DashboardPage />} />
            <Route
              path="/storico"
              element={
                <Suspense fallback={pageFallback}>
                  <HistoryPage />
                </Suspense>
              }
            />
            <Route
              path="/decisioni"
              element={
                <Suspense fallback={pageFallback}>
                  <DecisionsPage />
                </Suspense>
              }
            />
            <Route
              path="/analisi"
              element={
                <Suspense fallback={pageFallback}>
                  <AnalysisPage />
                </Suspense>
              }
            />
            <Route
              path="/confronto-sem"
              element={
                <Suspense fallback={pageFallback}>
                  <ComparisonPage />
                </Suspense>
              }
            />
            <Route
              path="/batteria"
              element={
                <Suspense fallback={pageFallback}>
                  <BatteryPage />
                </Suspense>
              }
            />
            <Route
              path="/report"
              element={
                <Suspense fallback={pageFallback}>
                  <ReportPage />
                </Suspense>
              }
            />
            <Route
              path="/notifiche"
              element={
                <Suspense fallback={pageFallback}>
                  <NotificationsPage user={activeUser} />
                </Suspense>
              }
            />
            <Route
              path="/impostazioni"
              element={
                <Suspense fallback={pageFallback}>
                  <SettingsPage user={activeUser} />
                </Suspense>
              }
            />
          </Route>
        </Routes>
      </BrowserRouter>
    </ThemeErrorBoundary>
    </MotionConfig>
  );
}

export function App() {
  const qc = useQueryClient();

  const authQuery = useQuery({
    queryKey: ["auth-me"],
    queryFn: () => apiFetch<AuthUser & { authenticated?: boolean }>("/api/v1/auth/me"),
    retry: false,
  });

  const activeUser: AuthUser | null =
    authQuery.data && authQuery.data.authenticated !== false && authQuery.data.username ? authQuery.data : null;

  // Live WebSocket stream lives above ThemeProvider so switching themes never closes WS
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

  return (
    <ThemeProvider username={activeUser?.username ?? null}>
      <ThemedShell
        activeUser={activeUser}
        onLoginSuccess={(user) => {
          qc.setQueryData(["auth-me"], { ...user, authenticated: true });
        }}
        onLogout={handleLogout}
      />
    </ThemeProvider>
  );
}

export default App;
