import { useState } from "react";
import { NavLink, Outlet } from "react-router-dom";
import {
  Activity,
  BarChart3,
  BatteryCharging,
  ChevronLeft,
  ChevronRight,
  Cpu,
  FileText,
  GitCompareArrows,
  History,
  LayoutDashboard,
  LogOut,
  Menu,
  Moon,
  Scale,
  Server,
  Settings,
  ShieldCheck,
  Sun,
  X,
} from "lucide-react";
import type { AppShellProps } from "../classic/ClassicAppShell";
import { useTheme } from "../../theme/useTheme";

const NAV_GROUPS = [
  {
    group: "Monitoraggio",
    items: [
      { to: "/", label: "Dashboard", icon: LayoutDashboard },
      { to: "/storico", label: "Storico", icon: History },
      { to: "/decisioni", label: "Decisioni", icon: Activity },
    ],
  },
  {
    group: "Analisi",
    items: [
      { to: "/analisi", label: "Analisi", icon: BarChart3 },
      { to: "/confronto-sem", label: "Confronto SEM", icon: GitCompareArrows },
      { to: "/batteria", label: "Batteria", icon: BatteryCharging },
      { to: "/report", label: "Report", icon: FileText },
    ],
  },
  {
    group: "Sistema",
    items: [{ to: "/impostazioni", label: "Impostazioni", icon: Settings }],
  },
];

const FLAT_NAV = NAV_GROUPS.flatMap((g) => g.items);

function statusLabel(status?: string): string {
  if (status === "online") return "Online";
  if (status === "degraded") return "Avviso";
  if (status === "offline") return "Offline";
  return "N/D";
}

function dotClass(status?: string): string {
  if (status === "online") return "dot online";
  if (status === "degraded") return "dot degraded";
  if (status === "offline") return "dot offline";
  return "dot";
}

export function EmeraldAppShell({ user, system, theme, onToggleTheme, onLogout }: AppShellProps) {
  const ctxTheme = useTheme();
  const activeScheme = theme ?? ctxTheme.resolvedScheme;
  const handleToggleScheme = onToggleTheme ?? ctxTheme.toggleColorScheme;
  const [collapsed, setCollapsed] = useState(false);
  const [mobileMenuOpen, setMobileMenuOpen] = useState(false);
  const [mobileSystemDrawerOpen, setMobileSystemDrawerOpen] = useState(false);

  const comps = system?.components ?? {};
  const espStatus = comps.esp32?.status;
  const mqttStatus = comps.mqtt?.status;
  const haStatus = comps.home_assistant?.status;
  const anyOffline = Object.values(comps).some((c) => c?.status === "offline");

  return (
    <div
      className={`em-shell ${collapsed ? "em-shell-collapsed" : ""}`}
      data-ui-template="emerald"
    >
      <a href="#main-content" className="skip-link">
        Salta al contenuto principale
      </a>

      {/* DESKTOP SIDEBAR (~240 px, prominent logo, grouped nav, connections + SHADOW footer) */}
      <aside className="em-sidebar" aria-label="Menu principale Emerald Control">
        <div className="em-sidebar-top">
          <div className="em-brand">
            <div className="em-brand-left">
              <div className="em-brand-mark" aria-hidden="true">
                <svg width="24" height="24" viewBox="0 0 24 24" fill="none">
                  <circle cx="17" cy="6" r="3" fill="#F7C948" />
                  <path d="M3 18h14l3-7H6l-3 7z" stroke="#34D6A0" strokeWidth="1.9" strokeLinejoin="round" />
                  <path d="M10 11l-2 7M14 11l-2 7" stroke="#34D6A0" strokeWidth="1.6" />
                </svg>
              </div>
              {!collapsed && (
                <div>
                  <div className="em-brand-title">Afore EMS</div>
                  <div className="em-brand-sub">Home Energy Management</div>
                </div>
              )}
            </div>
            <button
              type="button"
              className="btn em-collapse-btn"
              onClick={() => setCollapsed((v) => !v)}
              aria-label={collapsed ? "Espandi menu laterale" : "Comprimi menu laterale"}
            >
              {collapsed ? <ChevronRight size={15} /> : <ChevronLeft size={15} />}
            </button>
          </div>

          <nav className="em-nav" aria-label="Destinazioni principali">
            {NAV_GROUPS.map((grp) => (
              <div key={grp.group} className="em-nav-group">
                {!collapsed && <div className="em-nav-group-label">{grp.group}</div>}
                {grp.items.map(({ to, label, icon: Icon }) => (
                  <NavLink
                    key={to}
                    to={to}
                    end={to === "/"}
                    className={({ isActive }) => `em-nav-item ${isActive ? "active" : ""}`}
                    title={label}
                  >
                    <Icon size={18} aria-hidden="true" />
                    {!collapsed && <span>{label}</span>}
                  </NavLink>
                ))}
              </div>
            ))}
          </nav>
        </div>

        <div className="em-sidebar-footer">
          {!collapsed && (
            <>
              <div className="em-conn-section">
                <div className="em-conn-row">
                  <span className="conn-left">
                    <span className={dotClass(espStatus)} aria-hidden="true" />
                    <span>ESP32</span>
                  </span>
                  <span className="em-conn-meta">{statusLabel(espStatus)}</span>
                </div>
                <div className="em-conn-row">
                  <span className="conn-left">
                    <span className={dotClass(mqttStatus)} aria-hidden="true" />
                    <span>MQTT</span>
                  </span>
                  <span className="em-conn-meta">{mqttStatus === "online" ? "Connesso" : statusLabel(mqttStatus)}</span>
                </div>
                <div className="em-conn-row">
                  <span className="conn-left">
                    <span className={dotClass(haStatus)} aria-hidden="true" />
                    <span>Home Assistant</span>
                  </span>
                  <span className="em-conn-meta">{statusLabel(haStatus)}</span>
                </div>
              </div>

              <div className="em-shadow-box" role="note">
                <div className="em-shadow-box-title">
                  <ShieldCheck size={14} aria-hidden="true" />
                  <span>SHADOW · SOLA LETTURA</span>
                </div>
                <div className="em-shadow-box-sub">Decisioni simulate · 0 scritture Modbus</div>
              </div>
            </>
          )}

          <div className="em-sidebar-actions">
            <button
              type="button"
              className="btn em-side-btn"
              onClick={handleToggleScheme}
              aria-label="Cambia tema chiaro/scuro"
            >
              {activeScheme === "dark" ? <Sun size={14} /> : <Moon size={14} />}
              {!collapsed && <span>{activeScheme === "dark" ? "Chiaro" : "Scuro"}</span>}
            </button>
            <button
              type="button"
              className="btn em-side-btn-icon"
              onClick={onLogout}
              aria-label={`Esci (${user.username})`}
              title={`Esci (${user.username} · ${user.role})`}
            >
              <LogOut size={14} />
            </button>
            {!collapsed && <span className="em-version-tag">v0.1.0</span>}
          </div>
        </div>
      </aside>

      {/* MAIN CONTENT AREA */}
      <div className="em-main-wrap">
        {/* Compact Emerald Mobile Header with Stato Impianto Drawer trigger */}
        <header className="em-mobile-header">
          <div className="em-mobile-header-left">
            <button
              type="button"
              className="btn em-icon-btn"
              onClick={() => setMobileMenuOpen(true)}
              aria-label="Apri menu navigazione"
            >
              <Menu size={18} />
            </button>
            <div>
              <div className="em-mobile-title">Afore EMS</div>
              <div className="em-mobile-sub">Emerald Control</div>
            </div>
          </div>
          <div className="em-mobile-header-right">
            <button
              type="button"
              className="btn em-mobile-sys-btn"
              onClick={() => setMobileSystemDrawerOpen(true)}
              aria-label="Apri stato impianto"
            >
              <Server size={14} />
              <span>Impianto</span>
            </button>
            <span className="em-status-pill em-tone-shadow">SHADOW</span>
          </div>
        </header>

        {/* Mobile Navigation Drawer */}
        {mobileMenuOpen && (
          <div className="mobile-drawer-backdrop" onClick={() => setMobileMenuOpen(false)}>
            <div className="mobile-drawer em-mobile-drawer" onClick={(e) => e.stopPropagation()} role="dialog" aria-label="Menu Emerald">
              <div className="em-mobile-drawer-head">
                <div>
                  <strong>Afore EMS · {user.username}</strong>
                  <div className="em-version-tag">Emerald Control v0.1.0</div>
                </div>
                <button type="button" className="btn em-icon-btn" onClick={() => setMobileMenuOpen(false)} aria-label="Chiudi menu">
                  <X size={16} />
                </button>
              </div>
              <nav className="em-nav">
                {FLAT_NAV.map(({ to, label, icon: Icon }) => (
                  <NavLink
                    key={to}
                    to={to}
                    end={to === "/"}
                    className={({ isActive }) => `em-nav-item ${isActive ? "active" : ""}`}
                    onClick={() => setMobileMenuOpen(false)}
                  >
                    <Icon size={18} />
                    <span>{label}</span>
                  </NavLink>
                ))}
              </nav>
              <div className="em-mobile-drawer-actions">
                <button type="button" className="btn" onClick={handleToggleScheme}>
                  {activeScheme === "dark" ? <Sun size={15} /> : <Moon size={15} />}
                  <span>Tema {activeScheme === "dark" ? "Chiaro" : "Scuro"}</span>
                </button>
                <button type="button" className="btn" onClick={onLogout}>
                  <LogOut size={15} />
                  <span>Esci ({user.role})</span>
                </button>
              </div>
            </div>
          </div>
        )}

        {/* Mobile Stato Impianto Drawer (full-height on small viewports) */}
        {mobileSystemDrawerOpen && (
          <div className="em-drawer-backdrop" onClick={() => setMobileSystemDrawerOpen(false)}>
            <aside
              className="em-drawer-panel"
              role="dialog"
              aria-modal="true"
              aria-label="Stato impianto"
              onClick={(e) => e.stopPropagation()}
            >
              <div className="em-drawer-head">
                <div>
                  <h3 className="em-drawer-title">Stato impianto</h3>
                  <p className="em-drawer-sub">Connettività ESP32, MQTT, HA e sicurezza</p>
                </div>
                <button
                  type="button"
                  className="btn em-drawer-close"
                  onClick={() => setMobileSystemDrawerOpen(false)}
                  aria-label="Chiudi stato impianto"
                >
                  <X size={18} />
                </button>
              </div>
              <div className="em-drawer-body">
                <div className="em-node-detail-list">
                  <div className="em-node-detail-row">
                    <span className="em-node-detail-k">Stato generale</span>
                    <strong className="em-node-detail-v">{anyOffline ? "Attenzione" : "Operativo"}</strong>
                  </div>
                  <div className="em-node-detail-row">
                    <span className="em-node-detail-k">ESP32</span>
                    <strong className="em-node-detail-v">{statusLabel(espStatus)}</strong>
                  </div>
                  <div className="em-node-detail-row">
                    <span className="em-node-detail-k">MQTT</span>
                    <strong className="em-node-detail-v">{mqttStatus === "online" ? "Connesso" : statusLabel(mqttStatus)}</strong>
                  </div>
                  <div className="em-node-detail-row">
                    <span className="em-node-detail-k">Home Assistant</span>
                    <strong className="em-node-detail-v">{statusLabel(haStatus)}</strong>
                  </div>
                  <div className="em-node-detail-row">
                    <span className="em-node-detail-k">Modalità</span>
                    <strong className="em-node-detail-v">SHADOW · 0 scritture Modbus</strong>
                  </div>
                  <div className="em-node-detail-row">
                    <span className="em-node-detail-k">AUX1</span>
                    <strong className="em-node-detail-v">{String(comps.aux1?.detail?.aux1 ?? "OFF")}</strong>
                  </div>
                </div>
              </div>
            </aside>
          </div>
        )}

        <main id="main-content" className="em-main" tabIndex={-1}>
          <Outlet />
        </main>

        {/* Mobile Bottom Nav */}
        <nav className="mobile-bottom-nav em-bottom-nav" aria-label="Navigazione rapida Emerald mobile">
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
            <Cpu size={17} />
            <span>Altro</span>
          </button>
        </nav>
      </div>
    </div>
  );
}
