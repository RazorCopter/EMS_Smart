import type { EChartsOption } from "echarts";
import type { ChartPalette } from "../theme/themeTypes";
import {
  fmtPct,
  humanDecisionDetail,
  strategyLabel,
} from "../lib/format";
import { buildEnergyFlowModel, type DerivedEnergyFlow } from "../components/shared/energyFlowModel";

export type LiveChartRange = "1h" | "3h" | "6h" | "12h" | "today";

export interface LiveResponse {
  generated_at: string;
  house_meter?: { watts: number; received_at: string } | null;
  timezone: string;
  telemetry: any;
  decision: any;
  config?: any;
  kpi_today: any;
  kpi_yesterday: any;
  comparison_yesterday?: { available: boolean; reason: string };
  coverage?: {
    first_sample_at: string | null;
    effective_monitor_start_at: string | null;
    monitoring_started_today: boolean;
    unmonitored_pre_install_seconds: number;
    monitored_window_seconds: number;
    valid_seconds: number;
    gap_seconds: number;
    invalid_samples: number;
    unavailable_seconds: number;
    coverage_since_first_sample_pct: number | null;
    legacy_stride_gaps_excluded?: number;
  };
  sparklines: Array<{ hour: string; pv_kwh: number | null; load_kwh: number | null; chg_kwh: number | null; imp_kwh: number | null }>;
  system: { components: Record<string, { status: string; recorded_at: string | null; detail: Record<string, unknown> }> };
  grouped_events?: Array<{
    key: string;
    id: number;
    occurred_at: string;
    event_type: string;
    severity: string;
    title: string;
    detail: string | null;
    count: number;
  }>;
  recent_events: Array<{ id: number; occurred_at: string; event_type: string; severity: string; title: string; detail: string | null }>;
  resources?: {
    esp32?: Record<string, number | string | null | undefined>;
    containers?: {
      api?: Record<string, number | null | undefined>;
      collector?: Record<string, number | null | undefined>;
    };
    database?: {
      engine?: string;
      db_size_mb?: number | null;
      hypertables?: Array<{ name: string; size_mb: number; chunks: number; compressed_chunks: number }>;
      retention_policies?: Array<{ name: string; interval: string; last_status: string | null }>;
    };
  };
}

export type EnergyFlowViewModel = DerivedEnergyFlow;

export interface DecisionViewModel {
  configuredStrategyLabel: string;
  effectiveStrategyLabel: string;
  simulatedW: number | null;
  candidateW: number | null;
  limitedW: number | null;
  safetyMinW: number | null;
  safetyMaxW: number | null;
  safetyReasonCode: string | null;
  reasonCode: string | null;
  hasSafetyLimit: boolean;
  safetyStatusText: string;
  humanTitle: string;
  humanSubtitle: string;
  tone: "normal" | "safety" | "warn";
  timestamp: string | null;
  timezone: string;
  seq: number | null;
  detailUrl: string;
  socPct: number | null;
}

export interface SparkPoint {
  hour: string;
  value: number | null;
}

export interface DailyKpiViewModel {
  selfConsumptionPct: number | null;
  selfSufficiencyPct: number | null;
  pvNotCalculable: boolean;
  loadNotCalculable: boolean;
  pvKwh: number | null;
  loadKwh: number | null;
  selfConsumedKwh: number | null;
  gridImportKwh: number | null;
  gridExportKwh: number | null;
  batteryChargeKwh: number | null;
  batteryDischargeKwh: number | null;
  batteryThroughputKwh: number | null;
  batteryCycles: number | null;
  batteryVoltageV: number | null;
  socPct: number | null;
  yesterdayAvailable: boolean;
  yesterday: {
    selfConsumptionPct: number | null;
    selfSufficiencyPct: number | null;
    pvKwh: number | null;
    loadKwh: number | null;
    gridImportKwh: number | null;
    batteryChargeKwh: number | null;
  };
  sparks: {
    pv: SparkPoint[];
    load: SparkPoint[];
    grid: SparkPoint[];
    battery: SparkPoint[];
  };
}

export interface CoverageViewModel {
  pct: number | null;
  firstSampleAt: string | null;
  monitoringStartedToday: boolean;
  unmonitoredPreInstallSec: number;
  gapSeconds: number;
  invalidSamples: number;
  unavailableSeconds: number;
  legacyExcluded: number;
}

export interface SystemHealthViewModel {
  components: Record<string, { status: string; recorded_at: string | null; detail: Record<string, unknown> }>;
  anyOffline: boolean;
  resources: NonNullable<LiveResponse["resources"]>;
  modalitaEsp: string;
  controlloReale: string;
  aux1State: string;
  semOnline: boolean;
}

export interface LiveChartViewModel {
  title: string;
  option: EChartsOption;
}

export const CHART_RANGE_TITLES: Record<LiveChartRange, string> = {
  "1h": "Andamento ultima ora",
  "3h": "Andamento ultime 3 ore",
  "6h": "Andamento ultime 6 ore",
  "12h": "Andamento ultime 12 ore",
  today: "Andamento di oggi",
};

export interface DashboardViewModels {
  raw: LiveResponse;
  telemetry: any;
  telStateClass: "ok" | "warn" | "err";
  energyFlow: EnergyFlowViewModel;
  decision: DecisionViewModel;
  kpis: DailyKpiViewModel;
  coverage: CoverageViewModel;
  system: SystemHealthViewModel;
  liveChart: LiveChartViewModel;
  eventsTop3: Array<{ id?: number; key?: string; occurred_at: string; event_type: string; severity: string; title: string; detail: string | null }>;
  generatedAt: string;
  timezone: string;
}

export function buildLiveChartOption(
  series: any[],
  timezone: string,
  chartPalette: ChartPalette,
): EChartsOption {
  return {
    grid: { top: 34, right: 44, bottom: 26, left: 46 },
    tooltip: {
      trigger: "axis",
      confine: true,
      backgroundColor: chartPalette.tooltipBg,
      borderColor: chartPalette.tooltipBorder,
      borderWidth: 1,
      padding: [8, 12],
      textStyle: { color: chartPalette.textPrimary, fontSize: 12 },
      axisPointer: {
        type: "line",
        lineStyle: { color: "rgba(0, 240, 255, 0.35)", width: 1, type: "dashed" },
      },
    },
    legend: {
      top: 0,
      left: 0,
      itemWidth: 12,
      itemHeight: 8,
      textStyle: { color: chartPalette.axisLabel, fontSize: 11.5 },
    },
    xAxis: {
      type: "category",
      data: series.map((p) =>
        new Date(p.timestamp).toLocaleTimeString("it-IT", {
          timeZone: timezone || "Europe/Rome",
          hour: "2-digit",
          minute: "2-digit",
        }),
      ),
      axisLabel: { color: chartPalette.axisLabel, fontSize: 10.5 },
    },
    yAxis: [
      {
        type: "value",
        axisLabel: {
          color: chartPalette.axisLabel,
          fontSize: 10.5,
          formatter: (v: number) => (Math.abs(v) >= 1000 ? `${(v / 1000).toFixed(1)}k` : `${Math.round(v)} W`),
        },
        splitLine: { lineStyle: { color: chartPalette.splitLine, type: "dashed" } },
      },
      {
        type: "value",
        min: 0,
        max: 100,
        axisLabel: { color: chartPalette.axisLabel, fontSize: 10.5, formatter: "{value}%" },
        splitLine: { show: false },
      },
    ],
    series: [
      {
        name: "FV",
        type: "line",
        showSymbol: false,
        smooth: 0.2,
        connectNulls: false,
        lineStyle: { color: chartPalette.pv, width: 2.2 },
        itemStyle: { color: chartPalette.pv },
        areaStyle: { color: chartPalette.pvArea },
        data: series.map((p) => p.pv_w),
      },
      {
        name: "Casa",
        type: "line",
        showSymbol: false,
        smooth: 0.2,
        connectNulls: false,
        lineStyle: { color: chartPalette.home, width: 2 },
        itemStyle: { color: chartPalette.home },
        data: series.map((p) => p.load_w),
      },
      {
        name: "Batteria",
        type: "line",
        showSymbol: false,
        smooth: 0.2,
        connectNulls: false,
        lineStyle: { color: chartPalette.battery, width: 2 },
        itemStyle: { color: chartPalette.battery },
        data: series.map((p) => p.battery_w),
      },
      {
        name: "Rete",
        type: "line",
        showSymbol: false,
        smooth: 0.2,
        connectNulls: false,
        lineStyle: { color: chartPalette.grid, width: 1.8 },
        itemStyle: { color: chartPalette.grid },
        data: series.map((p) => p.grid_w),
      },
      {
        name: "SOC",
        type: "line",
        yAxisIndex: 1,
        showSymbol: false,
        connectNulls: false,
        lineStyle: { color: chartPalette.soc, width: 1.6, type: "dashed" },
        itemStyle: { color: chartPalette.soc },
        data: series.map((p) => p.soc),
      },
    ],
  };
}

export function buildDashboardViewModels(
  live: LiveResponse,
  histSeries: any[],
  chartPalette: ChartPalette,
  chartRange: LiveChartRange,
): DashboardViewModels {
  const tel = live.telemetry;
  const dec = live.decision;
  const kpi = live.kpi_today ?? {};
  const yest = live.kpi_yesterday ?? {};
  const cov = live.coverage;
  const sparksRaw = live.sparklines ?? [];
  const comps = live.system?.components ?? {};
  const socPct = tel?.soc ?? dec?.soc ?? null;
  const safetyReasonCode = dec?.safety_reason ?? null;
  const hasSafetyLimit = Boolean(safetyReasonCode && safetyReasonCode !== "NONE");

  const decisionReason = humanDecisionDetail(
    dec?.safety_reason,
    dec?.reason,
    socPct,
    dec?.safety_min_w,
    dec?.safety_max_w,
  );

  const energyFlow = buildEnergyFlowModel(
    tel,
    20,
    dec?.safety_min_w,
    dec?.safety_max_w,
    kpi.self_consumption_pct,
  );

  const decision: DecisionViewModel = {
    configuredStrategyLabel: strategyLabel(dec?.configured_strategy),
    effectiveStrategyLabel: strategyLabel(dec?.strategy),
    simulatedW: dec?.simulated_w ?? null,
    candidateW: dec?.candidate_w ?? null,
    limitedW: dec?.limited_w ?? null,
    safetyMinW: dec?.safety_min_w ?? null,
    safetyMaxW: dec?.safety_max_w ?? null,
    safetyReasonCode,
    reasonCode: dec?.reason ?? null,
    hasSafetyLimit,
    safetyStatusText: hasSafetyLimit
      ? decisionReason.title
      : "Nessuna limitazione",
    humanTitle: decisionReason.title,
    humanSubtitle: decisionReason.subtitle,
    tone: decisionReason.tone,
    timestamp: dec?.source_timestamp ?? dec?.timestamp ?? null,
    timezone: live.timezone,
    seq: dec?.seq ?? null,
    detailUrl: dec?.id ? `/decisioni?id=${encodeURIComponent(dec.id)}` : "/decisioni",
    socPct,
  };

  const kpis: DailyKpiViewModel = {
    selfConsumptionPct: kpi.self_consumption_pct ?? null,
    selfSufficiencyPct: kpi.self_sufficiency_pct ?? null,
    pvNotCalculable: kpi.pv_kwh == null || kpi.pv_kwh <= 0.001,
    loadNotCalculable: kpi.load_kwh == null || kpi.load_kwh <= 0.001,
    pvKwh: kpi.pv_kwh ?? null,
    loadKwh: kpi.load_kwh ?? null,
    selfConsumedKwh: kpi.self_consumed_pv_kwh ?? null,
    gridImportKwh: kpi.grid_import_kwh ?? null,
    gridExportKwh: kpi.grid_export_kwh ?? null,
    batteryChargeKwh: kpi.battery_charge_kwh ?? null,
    batteryDischargeKwh: kpi.battery_discharge_kwh ?? null,
    batteryThroughputKwh: kpi.battery_throughput_kwh ?? null,
    batteryCycles: kpi.equivalent_cycles ?? kpi.estimated_equivalent_cycles ?? null,
    batteryVoltageV: tel?.battery_v ?? null,
    socPct,
    yesterdayAvailable: Boolean(live.comparison_yesterday?.available),
    yesterday: {
      selfConsumptionPct: yest.self_consumption_pct ?? null,
      selfSufficiencyPct: yest.self_sufficiency_pct ?? null,
      pvKwh: yest.pv_kwh ?? null,
      loadKwh: yest.load_kwh ?? null,
      gridImportKwh: yest.grid_import_kwh ?? null,
      batteryChargeKwh: yest.battery_charge_kwh ?? null,
    },
    sparks: {
      pv: sparksRaw.map((p) => ({ hour: p.hour, value: p.pv_kwh })),
      load: sparksRaw.map((p) => ({ hour: p.hour, value: p.load_kwh })),
      grid: sparksRaw.map((p) => ({ hour: p.hour, value: p.imp_kwh })),
      battery: sparksRaw.map((p) => ({ hour: p.hour, value: p.chg_kwh })),
    },
  };

  const coverage: CoverageViewModel = {
    pct: cov?.coverage_since_first_sample_pct ?? kpi.data_availability_pct ?? null,
    firstSampleAt: cov?.first_sample_at ?? null,
    monitoringStartedToday: Boolean(cov?.monitoring_started_today),
    unmonitoredPreInstallSec: cov?.unmonitored_pre_install_seconds ?? 0,
    gapSeconds: cov?.gap_seconds ?? 0,
    invalidSamples: cov?.invalid_samples ?? 0,
    unavailableSeconds: cov?.unavailable_seconds ?? 0,
    legacyExcluded: cov?.legacy_stride_gaps_excluded ?? 0,
  };

  const semOnline = Boolean(
    comps.home_assistant?.status === "online" &&
      (live.config?.sem_topic_present || Object.keys(comps).includes("home_assistant")),
  );

  const auxDetail = comps.aux1?.detail ?? {};
  const isLiveMode = auxDetail.operating_mode === "LIVE";
  const writeCount = typeof auxDetail.modbus_write_count === "number" ? auxDetail.modbus_write_count : 0;

  const system: SystemHealthViewModel = {
    components: comps,
    anyOffline: Object.values(comps).some((c) => c?.status === "offline"),
    resources: live.resources ?? {},
    modalitaEsp: `${isLiveMode ? "LIVE" : "SHADOW"} · ${strategyLabel(dec?.strategy)}`,
    controlloReale: isLiveMode ? `Abilitato (${writeCount} scritture Modbus)` : `Disabilitato (${writeCount} scritture Modbus)`,
    aux1State: String(comps.aux1?.detail?.aux1 ?? "OFF"),
    semOnline,
  };

  return {
    raw: live,
    telemetry: tel,
    telStateClass: !tel ? "warn" : tel.stale ? "warn" : tel.fault ? "err" : "ok",
    energyFlow,
    decision,
    kpis,
    coverage,
    system,
    liveChart: {
      title: CHART_RANGE_TITLES[chartRange],
      option: buildLiveChartOption(histSeries, live.timezone, chartPalette),
    },
    eventsTop3: (live.grouped_events ?? live.recent_events ?? []).slice(0, 3),
    generatedAt: live.generated_at,
    timezone: live.timezone,
  };
}

export function formatDeltaYesterdayPct(today: number | null, yesterday: number | null, available: boolean): string {
  if (!available || today == null || yesterday == null) return "Confronto disponibile da domani";
  const diff = today - yesterday;
  const sign = diff >= 0 ? "+" : "";
  return `vs ieri: ${sign}${fmtPct(diff)}`;
}
