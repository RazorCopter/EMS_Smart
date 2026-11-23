import { version as companionVersion } from "../../../package.json";
import { useState } from "react";
import { NavLink, Outlet } from "react-router-dom";
import {
  Activity,
  BarChart3,
  BatteryCharging,
  ChevronLeft,
  ChevronRight,
  FileText,
  GitCompareArrows,
  History,
  LayoutDashboard,
  LogOut,
  Menu,
  Moon,
  Scale,
  Settings,
  Sun,
  X,
} from "lucide-react";
import type { AuthUser } from "../../lib/api";
import { useTheme } from "../../theme/useTheme";

export interface ShellSystemStatus {
  components?: Record<string, { status: string; detail?: Record<string, unknown> }>;
}

export interface AppShellProps {
  user: AuthUser;
  system: ShellSystemStatus | null;
  theme?: "dark" | "light";
  onToggleTheme?: () => void;
  onLogout: () => void;
}

const NAV_ITEMS = [
  { to: "/", label: "Dashboard", icon: LayoutDashboard },
  { to: "/storico", label: "Storico", icon: History },
  { to: "/decisioni", label: "Decisioni", icon: Activity },
  { to: "/analisi", label: "Analisi", icon: BarChart3 },
  { to: "/confronto-sem", label: "Confronto SEM", icon: GitCompareArrows },
  { to: "/batteria", label: "Batteria", icon: BatteryCharging },
  { to: "/report", label: "Report", icon: FileText },
  { to: "/impostazioni", label: "Impostazioni", icon: Settings },
];

function dotClass(status?: string): string {
  if (status === "online") return "dot online";
  if (status === "degraded") return "dot degraded";
  if (status === "offline") return "dot offline";
  return "dot";
}

function statusText(status?: string): string {
  if (status === "online") return "Online";
  if (status === "degraded") return "Degradato";
  if (status === "offline") return "Offline";
  return "Sconosciuto";
}

export function ClassicAppShell({ user, system, theme, onToggleTheme, onLogout }: AppShellProps) {
  const ctxTheme = useTheme();
  const activeScheme = theme ?? ctxTheme.resolvedScheme;
  const handleToggleScheme = onToggleTheme ?? ctxTheme.toggleColorScheme;
  const [collapsed, setCollapsed] = useState(false);
  const [mobileMenuOpen, setMobileMenuOpen] = useState(false);

  const comps = system?.components ?? {};
  const espStatus = comps.esp32?.status;
  const mqttStatus = comps.mqtt?.status;
  const haStatus = comps.home_assistant?.status;

  return (
    <div className={`app-shell ${collapsed ? "sidebar-collapsed" : ""}`} data-ui-template="classic">
      <a href="#main-content" className="skip-link">
        Salta al contenuto principale
      </a>
      {/* Desktop Sidebar */}
      <aside className="sidebar" aria-label="Menu principale">
        <div>
          <div className="brand">
            <div className="brand-left">
              <div className="brand-mark" aria-hidden="true">
                <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="#2dd4bf" strokeWidth="2">
                  <circle cx="17" cy="6" r="3" fill="#f5b942" stroke="#f5b942" />
                  <path d="M3 18h14l3-7H6l-3 7z" />
                  <path d="M10 11l-2 7M14 11l-2 7" />
                </svg>
              </div>
              {!collapsed && (
                <div>
                  <div className="brand-title">Afore EMS</div>
                  <div className="brand-sub">Companion · v{companionVersion}</div>
                </div>
              )}
            </div>
            <button
              type="button"
              className="btn"
              style={{ padding: 6, minHeight: 30 }}
              onClick={() => setCollapsed((v) => !v)}
              aria-label={collapsed ? "Espandi menu laterale" : "Comprimi menu laterale"}
            >
              {collapsed ? <ChevronRight size={15} /> : <ChevronLeft size={15} />}
            </button>
          </div>

          <nav className="nav">
            {NAV_ITEMS.map(({ to, label, icon: Icon }) => (
              <NavLink
                key={to}
                to={to}
                end={to === "/"}
                className={({ isActive }) => `nav-item ${isActive ? "active" : ""}`}
                title={label}
              >
                <Icon size={18} aria-hidden="true" />
                {!collapsed && <span>{label}</span>}
              </NavLink>
            ))}
          </nav>
        </div>

        <div className="sidebar-footer">
          {!collapsed && (
            <>
              <div className="conn-row">
                <span className="conn-left">
                  <span className={dotClass(espStatus)} aria-hidden="true" />
                  ESP32
                </span>
                <span className="conn-meta">{statusText(espStatus)}</span>
              </div>
              <div className="conn-row">
                <span className="conn-left">
                  <span className={dotClass(mqttStatus)} aria-hidden="true" />
                  MQTT
                </span>
                <span className="conn-meta">{mqttStatus === "online" ? "Connesso" : statusText(mqttStatus)}</span>
              </div>
              <div className="conn-row">
                <span className="conn-left">
                  <span className={dotClass(haStatus)} aria-hidden="true" />
                  Home Assistant
                </span>
                <span className="conn-meta">{statusText(haStatus)}</span>
              </div>
              <div className="shadow-pill-box">
                <div className="shadow-pill-title">● SHADOW · SOLA LETTURA</div>
                <div className="shadow-pill-sub">Nessun comando applicato all&apos;inverter</div>
              </div>
            </>
          )}
          <div style={{ display: "flex", gap: 6, flexWrap: "wrap" }}>
            <button
              type="button"
              className="btn"
              style={{ flex: 1, padding: "6px 8px" }}
              onClick={handleToggleScheme}
              aria-label="Cambia tema chiaro/scuro"
            >
              {activeScheme === "dark" ? <Sun size={14} /> : <Moon size={14} />}
              {!collapsed && <span>{activeScheme === "dark" ? "Chiaro" : "Scuro"}</span>}
            </button>
            <button
              type="button"
              className="btn"
              style={{ padding: "6px 8px" }}
              onClick={onLogout}
              aria-label={`Esci (${user.username})`}
              title={`Esci (${user.username} · ${user.role})`}
            >
              <LogOut size={14} />
            </button>
          </div>
        </div>
      </aside>

      {/* Mobile Compact Header */}
      <div className="main-wrap">
        <header className="mobile-header">
          <div style={{ display: "flex", alignItems: "center", gap: 9 }}>
            <button
              type="button"
              className="btn"
              style={{ padding: 6, minHeight: 36 }}
              onClick={() => setMobileMenuOpen(true)}
              aria-label="Apri menu navigazione"
            >
              <Menu size={18} />
            </button>
            <div>
              <div style={{ fontWeight: 700, fontSize: 14 }}>Afore EMS Companion</div>
              <div style={{ fontSize: 11, color: "var(--text-secondary)" }}>
                ESP32: {statusText(espStatus)} · MQTT: {statusText(mqttStatus)}
              </div>
            </div>
          </div>
          <span className="chip shadow" style={{ padding: "4px 9px", fontSize: 11 }}>
            SHADOW
          </span>
        </header>

        {/* Mobile Drawer */}
        {mobileMenuOpen && (
          <div className="mobile-drawer-backdrop" onClick={() => setMobileMenuOpen(false)}>
            <div className="mobile-drawer" onClick={(e) => e.stopPropagation()} role="dialog" aria-label="Menu">
              <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 14 }}>
                <strong>Afore EMS · {user.username}</strong>
                <button type="button" className="btn" onClick={() => setMobileMenuOpen(false)} aria-label="Chiudi menu">
                  <X size={16} />
                </button>
              </div>
              <nav className="nav">
                {NAV_ITEMS.map(({ to, label, icon: Icon }) => (
                  <NavLink
                    key={to}
                    to={to}
                    end={to === "/"}
                    className={({ isActive }) => `nav-item ${isActive ? "active" : ""}`}
                    onClick={() => setMobileMenuOpen(false)}
                  >
                    <Icon size={18} />
                    <span>{label}</span>
                  </NavLink>
                ))}
              </nav>
              <div style={{ marginTop: 18, display: "flex", flexDirection: "column", gap: 8 }}>
                <button type="button" className="btn" onClick={handleToggleScheme}>
                  {activeScheme === "dark" ? <Sun size={15} /> : <Moon size={15} />} Tema {activeScheme === "dark" ? "Chiaro" : "Scuro"}
                </button>
                <button type="button" className="btn" onClick={onLogout}>
                  <LogOut size={15} /> Esci ({user.role})
                </button>
              </div>
            </div>
          </div>
        )}

        <main id="main-content" className="main" tabIndex={-1}>
          <Outlet />
        </main>

        {/* Mobile Bottom Nav (5 primary destinations) */}
        <nav className="mobile-bottom-nav" aria-label="Navigazione rapida mobile">
          <NavLink to="/" end className={({ isActive }) => `mobile-bottom-item ${isActive ? "active" : ""}`}>
            <LayoutDashboard size={17} />
            <span>Live</span>
          </NavLink>
          <NavLink to="/storico" className={({ isActive }) => `mobile-bottom-item ${isActive ? "active" : ""}`}>
            <History size={17} />
            <span>Storico</span>
          </NavLink>
          <NavLink to="/decisioni" className={({ isActive }) => `mobile-bottom-item ${isActive ? "active" : ""}`}>
            <Activity size={17} />
            <span>Decisioni</span>
          </NavLink>
          <NavLink to="/confronto-sem" className={({ isActive }) => `mobile-bottom-item ${isActive ? "active" : ""}`}>
            <Scale size={17} />
            <span>SEM</span>
          </NavLink>
          <button type="button" className="mobile-bottom-item" onClick={() => setMobileMenuOpen(true)}>
            <Menu size={17} />
            <span>Altro</span>
          </button>
        </nav>
      </div>
    </div>
  );
}
