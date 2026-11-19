import type { ReactNode } from "react";
import { useOptionalTheme } from "../../theme/useTheme";
import { EmeraldPageHeader } from "../emerald/EmeraldChrome";

interface Props {
  title: string;
  subtitle?: string;
  badges?: ReactNode;
  actions?: ReactNode;
}

export function PageHeader({ title, subtitle, badges, actions }: Props) {
  const { effectiveStyle } = useOptionalTheme();

  if (effectiveStyle === "emerald") {
    return (
      <EmeraldPageHeader
        title={title}
        subtitle={subtitle}
        badges={badges}
        actions={actions}
      />
    );
  }

  return (
    <header className="topbar">
      <div>
        <h1 className="page-title">{title}</h1>
        {subtitle && <p className="page-sub">{subtitle}</p>}
      </div>
      {(badges || actions) && (
        <div className="topbar-right">
          {badges}
          {actions}
        </div>
      )}
    </header>
  );
}
