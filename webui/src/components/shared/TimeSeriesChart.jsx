import React, { useRef, useEffect, useState } from 'react';
import uPlot from 'uplot';
import 'uplot/dist/uPlot.min.css';
import './TimeSeriesChart.css';

export default function TimeSeriesChart({ title, data, series = [], width: propWidth, height = 200, unit = '' }) {
  const containerRef = useRef(null);
  const chartRef = useRef(null);
  const [containerWidth, setContainerWidth] = useState(propWidth || 400);

  // ResizeObserver for responsive width
  useEffect(() => {
    if (propWidth) return;
    const el = containerRef.current;
    if (!el) return;
    const ro = new ResizeObserver((entries) => {
      for (const entry of entries) {
        const w = entry.contentRect.width - 12; // account for padding
        if (w > 0) setContainerWidth(w);
      }
    });
    ro.observe(el);
    return () => ro.disconnect();
  }, [propWidth]);

  const effectiveWidth = propWidth || containerWidth;

  // Create / recreate chart
  useEffect(() => {
    const el = containerRef.current;
    if (!el || effectiveWidth < 50) return;

    // Build uPlot series config
    const uSeries = [
      { label: 'Time' },
      ...series.map((s) => ({
        label: s.label || '',
        stroke: s.color || 'var(--accent)',
        width: s.width || 1.5,
        fill: s.fill || undefined,
      })),
    ];

    // If no series config provided, add a default
    if (series.length === 0) {
      uSeries.push({
        label: 'Value',
        stroke: 'var(--accent)',
        width: 1.5,
      });
    }

    const opts = {
      width: effectiveWidth,
      height,
      cursor: {
        show: true,
        drag: { x: false, y: false },
      },
      select: { show: false },
      scales: {
        x: { time: true },
        y: { auto: true },
      },
      axes: [
        {
          stroke: 'rgba(255,255,255,0.2)',
          grid: { stroke: 'rgba(255,255,255,0.04)', width: 1 },
          ticks: { stroke: 'rgba(255,255,255,0.04)', width: 1 },
          font: '10px Inter, sans-serif',
          gap: 4,
        },
        {
          stroke: 'rgba(255,255,255,0.2)',
          grid: { stroke: 'rgba(255,255,255,0.04)', width: 1 },
          ticks: { stroke: 'rgba(255,255,255,0.04)', width: 1 },
          font: '10px Inter, sans-serif',
          gap: 8,
          label: unit || undefined,
          labelFont: '10px Inter, sans-serif',
          labelGap: 4,
        },
      ],
      series: uSeries,
    };

    // Destroy previous instance
    if (chartRef.current) {
      chartRef.current.destroy();
      chartRef.current = null;
    }

    // Need at least 2 data points to render
    const chartData = data && data[0] && data[0].length >= 2 ? data : [[], ...series.map(() => [])];
    if (chartData[0].length === 0) {
      // Don't create chart with empty data, just show placeholder
      return;
    }

    const chart = new uPlot(opts, chartData, el);
    chartRef.current = chart;

    return () => {
      chart.destroy();
      chartRef.current = null;
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [effectiveWidth, height, series.length]);

  // Update data without recreating chart
  useEffect(() => {
    if (chartRef.current && data && data[0] && data[0].length >= 2) {
      chartRef.current.setData(data);
    }
  }, [data]);

  return (
    <div className="ts-chart">
      {title && (
        <div className="ts-chart__header">
          <span className="ts-chart__title">{title}</span>
          {unit && <span className="ts-chart__title">{unit}</span>}
        </div>
      )}
      <div className="ts-chart__body" ref={containerRef} />
    </div>
  );
}
