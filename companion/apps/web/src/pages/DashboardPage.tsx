import { lazy, Suspense, useMemo, useState } from "react";
import { useQuery } from "@tanstack/react-query";
import { apiFetch } from "../lib/api";
import { useTheme } from "../theme/useTheme";
import { ThemeErrorBoundary } from "../components/shared/ThemeErrorBoundary";
import { ClassicDashboardLayout } from "../components/classic/ClassicDashboardLayout";
import {
  buildDashboardViewModels,
  type LiveChartRange,
  type LiveResponse,
} from "../viewmodels/dashboardViewModel";

const EmeraldDashboardLayout = lazy(() =>
  import("../components/emerald/EmeraldDashboardLayout").then((m) => ({
    default: m.EmeraldDashboardLayout,
  })),
);

export function DashboardPage() {
  const {
    chartPalette,
    effectiveStyle,
    activateTemporaryClassicFallback,
    resetToDefault,
  } = useTheme();
  const [chartRange, setChartRange] = useState<LiveChartRange>("6h");

  const { data: live, isLoading, error } = useQuery({
    queryKey: ["live"],
    queryFn: () => apiFetch<LiveResponse>("/api/v1/live"),
    refetchInterval: 5000,
  });

  const { data: hist } = useQuery({
    queryKey: ["history-live-compact", chartRange],
    queryFn: () => apiFetch<{ series: any[] }>(`/api/v1/history?range=${chartRange}&bucket=auto`),
    refetchInterval: 30000,
  });

  const vm = useMemo(() => {
    if (!live) return null;
    return buildDashboardViewModels(live, hist?.series ?? [], chartPalette, chartRange);
  }, [live, hist, chartPalette, chartRange]);

  if (isLoading) {
    return <div className="card">Caricamento telemetria live...</div>;
  }

  if (error || !live || !vm) {
    return (
      <div className="card" role="alert">
        Impossibile caricare la Dashboard Live: {error instanceof Error ? error.message : "Errore sconosciuto"}
      </div>
    );
  }

  if (effectiveStyle === "emerald") {
    return (
      <ThemeErrorBoundary
        styleName="emerald"
        onFallbackToClassic={activateTemporaryClassicFallback}
        onResetDefault={resetToDefault}
      >
        <Suspense fallback={<div className="card">Caricamento Dashboard Emerald Control...</div>}>
          <EmeraldDashboardLayout
            vm={vm}
            chartRange={chartRange}
            onChangeChartRange={setChartRange}
          />
        </Suspense>
      </ThemeErrorBoundary>
    );
  }

  return (
    <ClassicDashboardLayout
      vm={vm}
      chartRange={chartRange}
      onChangeChartRange={setChartRange}
    />
  );
}
