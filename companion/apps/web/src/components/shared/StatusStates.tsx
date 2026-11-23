import { AlertCircle, Clock3, Inbox } from "lucide-react";

export function EmptyState({
  title,
  detail,
}: {
  title: string;
  detail: string;
}) {
  return (
    <div className="card" role="status" style={{ display: "flex", alignItems: "center", gap: 12 }}>
      <Inbox size={18} color="var(--text-muted)" aria-hidden="true" />
      <div>
        <div style={{ fontWeight: 650, fontSize: 13.5 }}>{title}</div>
        <div className="card-sub">{detail}</div>
      </div>
    </div>
  );
}

export function StaleState({
  label = "Telemetria non aggiornata",
  detail,
}: {
  label?: string;
  detail: string;
}) {
  return (
    <div className="chip stale" role="status" title={detail}>
      <Clock3 size={13} aria-hidden="true" />
      <span>{label}</span>
    </div>
  );
}

export function ErrorState({
  title = "Errore di caricamento",
  detail,
}: {
  title?: string;
  detail: string;
}) {
  return (
    <div className="card" role="alert" style={{ borderColor: "var(--fault-red)", display: "flex", alignItems: "center", gap: 10 }}>
      <AlertCircle size={18} color="var(--fault-red)" aria-hidden="true" />
      <div>
        <div style={{ fontWeight: 650, color: "var(--fault-red)" }}>{title}</div>
        <div className="card-sub">{detail}</div>
      </div>
    </div>
  );
}
