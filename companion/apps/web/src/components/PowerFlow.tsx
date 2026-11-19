import { lazy, Suspense } from "react";
import { useOptionalTheme } from "../theme/useTheme";
import { ClassicEnergyMonitor } from "./classic/ClassicEnergyMonitor";
import { ThemeErrorBoundary } from "./shared/ThemeErrorBoundary";

export interface TelemetryLive {
  age_s?: number | null;
  stale?: boolean;
  fault?: boolean | null;
  soc?: number | null;
  battery_w?: number | null;
  battery_v?: number | null;
  battery_a?: number | null;
  pv_w?: number | null;
  pv1_w?: number | null;
  pv1_v?: number | null;
  pv1_a?: number | null;
  pv2_w?: number | null;
  pv2_v?: number | null;
  pv2_a?: number | null;
  grid_w?: number | null;
  grid_v?: number | null;
  hz?: number | null;
  load_w?: number | null;
  temp_c?: number | null;
  reads?: number | null;
  errors?: number | null;
  quality?: Record<string, { age_ms?: number | null; source?: string; state?: string }>;
}

interface Props {
  telemetry: TelemetryLive | null;
  safetyMinW?: number | null;
  safetyMaxW?: number | null;
  selfConsumptionPct?: number | null;
  deadbandW?: number;
}

const EmeraldEnergyMonitor = lazy(() =>
  import("./emerald/EmeraldEnergyMonitor").then((m) => ({ default: m.EmeraldEnergyMonitor })),
);

export function PowerFlow(props: Props) {
  const theme = useOptionalTheme();
  if (theme.effectiveStyle === "emerald") {
    return (
      <ThemeErrorBoundary
        styleName="emerald"
        onFallbackToClassic={theme.activateTemporaryClassicFallback}
        onResetDefault={theme.resetToDefault}
      >
        <Suspense fallback={<div className="card">Caricamento Energy Monitor Emerald...</div>}>
          <EmeraldEnergyMonitor {...props} />
        </Suspense>
      </ThemeErrorBoundary>
    );
  }
  return <ClassicEnergyMonitor {...props} />;
}
