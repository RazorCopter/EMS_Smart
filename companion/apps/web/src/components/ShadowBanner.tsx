import { ShieldAlert } from "lucide-react";

interface Props {
  compact?: boolean;
  extra?: string;
}

export function ShadowBanner({ compact = false, extra }: Props) {
  return (
    <div className="shadow-banner" role="status" aria-live="polite">
      <div style={{ display: "flex", alignItems: "center", gap: 10 }}>
        <ShieldAlert size={18} color="var(--shadow-blue)" aria-hidden="true" />
        <div>
          <strong>SHADOW – Decisioni simulate</strong>
          <div style={{ fontSize: compact ? 11 : 12, color: "var(--text-secondary)" }}>
            Nessun comando applicato all&apos;inverter · Scritture Modbus: 0
            {extra ? ` · ${extra}` : ""}
          </div>
        </div>
      </div>
      {!compact && <span className="badge simulated">SOLA LETTURA</span>}
    </div>
  );
}
