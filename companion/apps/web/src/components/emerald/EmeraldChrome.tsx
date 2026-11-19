import { useEffect, type ReactNode } from "react";
import { AlertTriangle, CheckCircle2, Info, X, XCircle } from "lucide-react";

export function EmeraldStatusPill({
  tone = "neutral",
  label,
  title,
}: {
  tone?: "ok" | "warn" | "err" | "shadow" | "info" | "neutral";
  label: string;
  title?: string;
}) {
  const Icon =
    tone === "ok"
      ? CheckCircle2
      : tone === "warn"
      ? AlertTriangle
      : tone === "err"
      ? XCircle
      : Info;

  return (
    <span className={`em-status-pill em-tone-${tone}`} title={title}>
      <Icon size={13} aria-hidden="true" />
      <span>{label}</span>
    </span>
  );
}

export function EmeraldPageHeader({
  title,
  subtitle,
  badges,
  actions,
}: {
  title: string;
  subtitle?: string;
  badges?: ReactNode;
  actions?: ReactNode;
}) {
  return (
    <header className="em-page-header" data-ui-template="emerald">
      <div className="em-page-header-main">
        <div className="em-page-header-titles">
          <h1 className="em-page-title">{title}</h1>
          {subtitle && <p className="em-page-sub">{subtitle}</p>}
        </div>
        {(badges || actions) && (
          <div className="em-page-header-right">
            {badges && <div className="em-header-badges">{badges}</div>}
            {actions && <div className="em-header-actions">{actions}</div>}
          </div>
        )}
      </div>
    </header>
  );
}

export function EmeraldCard({
  title,
  subtitle,
  right,
  children,
  className = "",
}: {
  title?: string;
  subtitle?: string;
  right?: ReactNode;
  children: ReactNode;
  className?: string;
}) {
  return (
    <section className={`em-card ${className}`.trim()}>
      {(title || right) && (
        <div className="em-card-head">
          <div>
            {title && <h2 className="em-card-title">{title}</h2>}
            {subtitle && <p className="em-card-sub">{subtitle}</p>}
          </div>
          {right}
        </div>
      )}
      {children}
    </section>
  );
}

export function EmeraldChartContainer({
  title,
  subtitle,
  controls,
  footer,
  children,
}: {
  title: string;
  subtitle?: string;
  controls?: ReactNode;
  footer?: ReactNode;
  children: ReactNode;
}) {
  return (
    <section className="em-card em-chart-card">
      <div className="em-card-head em-chart-head">
        <div>
          <h3 className="em-card-title">{title}</h3>
          {subtitle && <p className="em-card-sub">{subtitle}</p>}
        </div>
        {controls && <div className="em-chart-controls">{controls}</div>}
      </div>
      <div className="em-chart-body">{children}</div>
      {footer && <div className="em-chart-footer">{footer}</div>}
    </section>
  );
}

export function EmeraldFilterBar({ children, label = "Filtri" }: { children: ReactNode; label?: string }) {
  return (
    <div className="em-filter-bar" role="region" aria-label={label}>
      {children}
    </div>
  );
}

export function EmeraldTable({ children, caption }: { children: ReactNode; caption?: string }) {
  return (
    <div className="em-table-wrap">
      <table className="data-table em-table">
        {caption && <caption className="sr-only">{caption}</caption>}
        {children}
      </table>
    </div>
  );
}

export function EmeraldEmptyState({
  title,
  description,
  action,
}: {
  title: string;
  description: string;
  action?: ReactNode;
}) {
  return (
    <div className="em-empty-state" role="status">
      <Info size={20} aria-hidden="true" />
      <div className="em-empty-title">{title}</div>
      <p className="em-empty-desc">{description}</p>
      {action}
    </div>
  );
}

export function EmeraldDetailDrawer({
  open,
  title,
  subtitle,
  accentColor,
  onClose,
  children,
}: {
  open: boolean;
  title: string;
  subtitle?: string;
  accentColor?: string;
  onClose: () => void;
  children: ReactNode;
}) {
  useEffect(() => {
    if (!open) return;
    function onKey(e: KeyboardEvent) {
      if (e.key === "Escape") onClose();
    }
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [open, onClose]);

  if (!open) return null;

  return (
    <div className="em-drawer-backdrop" onClick={onClose}>
      <aside
        className="em-drawer-panel"
        role="dialog"
        aria-modal="true"
        aria-label={title}
        onClick={(e) => e.stopPropagation()}
        style={accentColor ? { borderTopColor: accentColor } : undefined}
      >
        <div className="em-drawer-head">
          <div>
            <h3 className="em-drawer-title">{title}</h3>
            {subtitle && <p className="em-drawer-sub">{subtitle}</p>}
          </div>
          <button type="button" className="btn em-drawer-close" onClick={onClose} aria-label={`Chiudi ${title}`}>
            <X size={18} />
          </button>
        </div>
        <div className="em-drawer-body">{children}</div>
      </aside>
    </div>
  );
}
