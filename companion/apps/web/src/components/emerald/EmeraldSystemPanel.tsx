import {
  Activity,
  CheckCircle2,
  Cpu,
  Database,
  HardDrive,
  Home,
  Lock,
  Radio,
  Server,
  ShieldAlert,
  WifiOff,
  XCircle,
} from "lucide-react";
import type { SystemHealthViewModel } from "../../viewmodels/dashboardViewModel";
import { HelpTip } from "../HelpTip";
import { espControlStatus } from "./espModeLabel";

interface Props {
  system: SystemHealthViewModel;
  variant?: "panel" | "drawer";
}

function statusMeta(status?: string): { tone: "ok" | "warn" | "err" | "neutral"; label: string } {
  if (status === "online") return { tone: "ok", label: "Attivo" };
  if (status === "degraded") return { tone: "warn", label: "Degradato" };
  if (status === "offline") return { tone: "err", label: "Offline" };
  return { tone: "neutral", label: "N/D" };
}

function StatusIcon({ tone }: { tone: "ok" | "warn" | "err" | "shadow" | "neutral" }) {
  if (tone === "ok" || tone === "shadow") return <CheckCircle2 size={14} aria-hidden="true" />;
  if (tone === "warn") return <ShieldAlert size={14} aria-hidden="true" />;
  if (tone === "err") return <XCircle size={14} aria-hidden="true" />;
  return <WifiOff size={14} aria-hidden="true" />;
}

export function EmeraldSystemPanel({ system, variant = "panel" }: Props) {
  const comps = system.components ?? {};
  const espMeta = statusMeta(comps.esp32?.status);
  const syncMeta = statusMeta(comps.esp32_sync?.status);
  const mqttMeta = statusMeta(comps.mqtt?.status);
  const haMeta = statusMeta(comps.home_assistant?.status);
  const ntpMeta = statusMeta(comps.ntp?.status);
  const espStatus = espControlStatus(comps.aux1);

  const items = [
    { key: "esp32", icon: Cpu, label: "ESP32", tone: espMeta.tone, value: espMeta.label === "Attivo" ? "Online" : espMeta.label },
    { key: "esp32_sync", icon: Database, label: "Sincronizzazione ESP", tone: syncMeta.tone, value: syncMeta.label },
    { key: "mqtt", icon: Radio, label: "MQTT", tone: mqttMeta.tone, value: comps.mqtt?.status === "online" ? "Connesso" : mqttMeta.label },
    { key: "ha", icon: Home, label: "Home Assistant", tone: haMeta.tone, value: haMeta.label === "Attivo" ? "Online" : haMeta.label },
    { key: "ctrl", icon: Lock, label: "Controllo reale", tone: espStatus.realControlTone, value: espStatus.realControlValue },
    { key: "sem", icon: Server, label: "SEM", tone: system.semOnline ? ("ok" as const) : ("neutral" as const), value: system.semOnline ? "Attivo" : "N/D" },
    { key: "mode", icon: Activity, label: "Modalità ESP", tone: espStatus.modeTone, value: espStatus.modeLabel.replace(/^ESP · /, "") },
    { key: "aux", icon: HardDrive, label: "AUX", tone: system.aux1State !== "OFF" ? ("ok" as const) : ("neutral" as const), value: system.aux1State },
    { key: "ntp", icon: Database, label: "NTP", tone: ntpMeta.tone, value: comps.ntp?.status === "online" ? "Sync" : ntpMeta.label },
  ];

  const esp = system.resources?.esp32 ?? {};
  const db = system.resources?.database ?? {};
  const api = system.resources?.containers?.api ?? {};
  const fmtKb = (b: unknown) => (typeof b === "number" ? `${Math.round(b / 1024)} KB` : "N/D");

  return (
    <article className={`em-card em-system-panel em-system-${variant}`} aria-label="Stato impianto">
      <div className="em-card-head">
        <div>
          <h3 className="em-card-title">Stato impianto</h3>
          <p className="em-card-sub">Connettività e sicurezza locale</p>
        </div>
        <span className={`em-status-pill ${system.anyOffline ? "em-tone-warn" : "em-tone-ok"}`}>
          <StatusIcon tone={system.anyOffline ? "warn" : "ok"} />
          <span>{system.anyOffline ? "Verifica" : "Operativo"}</span>
        </span>
      </div>

      <div className="em-sys-list">
        {items.map(({ key, icon: Icon, label, tone, value }) => (
          <div key={key} className="em-sys-row">
            <span className="em-sys-key">
              <Icon size={14} aria-hidden="true" />
              <span>{label}</span>
            </span>
            <span className={`em-sys-val em-tone-text-${tone}`}>
              <StatusIcon tone={tone} />
              <span>{value}</span>
            </span>
          </div>
        ))}
      </div>

      <div className="em-sys-resources" aria-label="Risorse ESP32 e Container">
        <div className="em-res-chip">
          <span className="em-res-label">
            CPU ESP32
            <HelpTip
              id={`tip-em-cpu-${variant}`}
              title="CPU ESP32"
              text="Carico medio FreeRTOS dual-core dell'ESP32-S3."
              align="right"
            />
          </span>
          <strong>{typeof esp.cpu_pct === "number" ? `${esp.cpu_pct.toFixed(1)}%` : "N/D"}</strong>
        </div>
        <div className="em-res-chip">
          <span className="em-res-label">
            RAM ESP32
            <HelpTip
              id={`tip-em-ram-${variant}`}
              title="SRAM / PSRAM"
              text="Uso SRAM interna + PSRAM circolare da 8 MB."
              align="right"
            />
          </span>
          <strong>{typeof esp.heap_used_pct === "number" ? `${esp.heap_used_pct.toFixed(1)}%` : fmtKb(esp.heap_free)}</strong>
        </div>
        <div className="em-res-chip">
          <span className="em-res-label">
            Flash NVS
            <HelpTip
              id={`tip-em-nvs-${variant}`}
              title="Partizione NVS"
              text="Solo configurazione utente; zero log su Flash."
              align="right"
            />
          </span>
          <strong>{typeof esp.nvs_used_pct === "number" ? `${esp.nvs_used_pct.toFixed(1)}%` : "N/D"}</strong>
        </div>
        <div className="em-res-chip">
          <span className="em-res-label">
            DB / Disco
            <HelpTip
              id={`tip-em-db-${variant}`}
              title="TimescaleDB"
              text={`Dimensione DB ${typeof db.db_size_mb === "number" ? `${db.db_size_mb.toFixed(1)} MB` : "N/D"} · Disco ${typeof api.disk_used_pct === "number" ? `${api.disk_used_pct.toFixed(0)}%` : "N/D"}.`}
              align="right"
            />
          </span>
          <strong>{typeof db.db_size_mb === "number" ? `${db.db_size_mb.toFixed(1)} MB` : "N/D"}</strong>
        </div>
      </div>
    </article>
  );
}
