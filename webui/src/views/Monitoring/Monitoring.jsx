import React, { useState, useCallback, useEffect, useRef } from 'react';
import { api } from '../../services/api';
import { usePolling } from '../../hooks/usePolling';
import { useTimeSeries, useMultiTimeSeries } from '../../hooks/useTimeSeries';
import TimeSeriesChart from '../../components/shared/TimeSeriesChart';
import './Monitoring.css';

const RANGES = [
  { label: '1m', points: 60 },
  { label: '5m', points: 300 },
  { label: '30m', points: 1800 },
];

const SINK_COLORS = [
  '#4a90e2', '#00d4aa', '#e8a855', '#e85555', '#b266e8',
  '#55b8e8', '#e855a8', '#7ae855', '#e8d455', '#55e8c7',
];

export default function Monitoring() {
  const [range, setRange] = useState(1); // index into RANGES
  const maxPoints = RANGES[range].points;

  // PTP jitter time series
  const jitter = useTimeSeries(maxPoints);

  // Sink tracking
  const [sinkList, setSinkList] = useState([]);
  const sinkMinTime = useMultiTimeSeries(Math.max(sinkList.length, 1), maxPoints);
  const sinkErrors = useMultiTimeSeries(Math.max(sinkList.length, 1), maxPoints);
  const prevSinkCountRef = useRef(0);

  // Fetch sinks list periodically
  const fetchSinks = useCallback(async () => {
    try {
      const data = await api.getSinks();
      const list = Array.isArray(data?.sinks) ? data.sinks : (Array.isArray(data) ? data : []);
      setSinkList(list.filter(s => s.enabled !== false));
    } catch { /* ignore */ }
  }, []);

  useEffect(() => { fetchSinks(); }, [fetchSinks]);
  usePolling(fetchSinks, 5000);

  // Poll PTP status every 1s
  const fetchPtp = useCallback(async () => {
    try {
      const data = await api.getPTPStatus();
      if (data && data.jitter != null) {
        jitter.push(Number(data.jitter));
      }
    } catch { /* ignore */ }
  }, [jitter]);

  usePolling(fetchPtp, 1000);

  // Poll sink statuses every 2s
  const fetchSinkStatuses = useCallback(async () => {
    if (sinkList.length === 0) return;
    try {
      const statuses = await Promise.all(
        sinkList.map(s => api.getSinkStatus(s.id).catch(() => null))
      );
      const minTimes = statuses.map(s => s?.min_time != null ? Number(s.min_time) : null);
      const errors = statuses.map(s => s?.sink_flags?.err_count != null ? Number(s.sink_flags.err_count) : (s?.error_count != null ? Number(s.error_count) : null));

      if (sinkList.length === prevSinkCountRef.current) {
        sinkMinTime.push(minTimes);
        sinkErrors.push(errors);
      }
      prevSinkCountRef.current = sinkList.length;
    } catch { /* ignore */ }
  }, [sinkList, sinkMinTime, sinkErrors]);

  usePolling(fetchSinkStatuses, 2000);

  // Build series configs
  const jitterSeries = [{ label: 'Jitter', color: '#00d4aa', width: 1.5 }];

  const sinkMinTimeSeries = sinkList.map((s, i) => ({
    label: s.name || `Sink ${s.id}`,
    color: SINK_COLORS[i % SINK_COLORS.length],
    width: 1.5,
  }));

  const sinkErrorSeries = sinkList.map((s, i) => ({
    label: s.name || `Sink ${s.id}`,
    color: SINK_COLORS[i % SINK_COLORS.length],
    width: 1.5,
  }));

  return (
    <div className="monitoring">
      <div className="monitoring__header">
        <h1 className="monitoring__title">Monitoring</h1>
        <div className="monitoring__range">
          {RANGES.map((r, i) => (
            <button
              key={r.label}
              className={`monitoring__range-btn ${i === range ? 'monitoring__range-btn--active' : ''}`}
              onClick={() => setRange(i)}
            >
              {r.label}
            </button>
          ))}
        </div>
      </div>

      <div className="monitoring__charts">
        <TimeSeriesChart
          title="PTP Jitter"
          data={jitter.data}
          series={jitterSeries}
          height={250}
          unit="\u03BCs"
        />

        {sinkList.length > 0 ? (
          <>
            <TimeSeriesChart
              title="Sink Min Arrival Time"
              data={sinkMinTime.data}
              series={sinkMinTimeSeries}
              height={250}
              unit="ms"
            />
            <TimeSeriesChart
              title="Sink Errors"
              data={sinkErrors.data}
              series={sinkErrorSeries}
              height={250}
              unit="errors"
            />
          </>
        ) : (
          <div className="monitoring__empty">No active sinks to monitor</div>
        )}
      </div>
    </div>
  );
}
