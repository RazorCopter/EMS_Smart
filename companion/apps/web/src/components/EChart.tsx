import { useEffect, useRef } from "react";
import * as echarts from "echarts";
import type { EChartsOption } from "echarts";

interface Props {
  option: EChartsOption;
  height?: number;
  ariaLabel: string;
  onChartReady?: (instance: echarts.ECharts) => void;
}

export function EChart({ option, height = 320, ariaLabel, onChartReady }: Props) {
  const containerRef = useRef<HTMLDivElement | null>(null);
  const chartRef = useRef<echarts.ECharts | null>(null);
  const optionRef = useRef<EChartsOption>(option);
  const readyRef = useRef(onChartReady);
  optionRef.current = option;
  readyRef.current = onChartReady;

  useEffect(() => {
    const el = containerRef.current;
    if (!el) return;
    const chart = echarts.init(el);
    chartRef.current = chart;
    chart.setOption(optionRef.current, { notMerge: false, lazyUpdate: true });
    readyRef.current?.(chart);

    const observer = new ResizeObserver(() => {
      chart.resize();
    });
    observer.observe(el);
    return () => {
      observer.disconnect();
      chart.dispose();
      chartRef.current = null;
    };
  }, []);

  useEffect(() => {
    // Update options in place without tearing down zoom or legend state on live WS / theme switches
    chartRef.current?.setOption(option, { notMerge: false, lazyUpdate: true });
  }, [option]);

  return (
    <div
      ref={containerRef}
      style={{ width: "100%", height }}
      role="img"
      aria-label={ariaLabel}
    />
  );
}
