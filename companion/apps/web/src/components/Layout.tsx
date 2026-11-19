import { lazy, Suspense } from "react";
import { useOptionalTheme } from "../theme/useTheme";
import { ClassicAppShell, type AppShellProps } from "./classic/ClassicAppShell";
import { ThemeErrorBoundary } from "./shared/ThemeErrorBoundary";

const EmeraldAppShell = lazy(() =>
  import("./emerald/EmeraldAppShell").then((m) => ({ default: m.EmeraldAppShell })),
);

export function Layout(props: AppShellProps) {
  const theme = useOptionalTheme();
  if (theme.effectiveStyle === "emerald") {
    return (
      <ThemeErrorBoundary
        styleName="emerald"
        onFallbackToClassic={theme.activateTemporaryClassicFallback}
        onResetDefault={theme.resetToDefault}
      >
        <Suspense fallback={<div className="card">Caricamento layout Emerald Control...</div>}>
          <EmeraldAppShell {...props} />
        </Suspense>
      </ThemeErrorBoundary>
    );
  }
  return <ClassicAppShell {...props} />;
}
