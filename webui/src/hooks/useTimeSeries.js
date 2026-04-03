import { useState, useCallback, useRef } from 'react';

export function useTimeSeries(maxPoints = 300) {
  const [data, setData] = useState([[], []]);
  const bufRef = useRef({ ts: [], vals: [] });

  const push = useCallback((value) => {
    const buf = bufRef.current;
    const now = Math.floor(Date.now() / 1000);
    buf.ts.push(now);
    buf.vals.push(value);
    if (buf.ts.length > maxPoints) {
      buf.ts = buf.ts.slice(-maxPoints);
      buf.vals = buf.vals.slice(-maxPoints);
    }
    setData([[...buf.ts], [...buf.vals]]);
  }, [maxPoints]);

  const clear = useCallback(() => {
    bufRef.current = { ts: [], vals: [] };
    setData([[], []]);
  }, []);

  return { data, push, clear };
}

export function useMultiTimeSeries(seriesCount, maxPoints = 300) {
  const initial = [[]];
  for (let i = 0; i < seriesCount; i++) initial.push([]);
  const [data, setData] = useState(initial);
  const bufRef = useRef({ ts: [], series: Array.from({ length: seriesCount }, () => []) });

  const push = useCallback((values) => {
    const buf = bufRef.current;
    const now = Math.floor(Date.now() / 1000);
    buf.ts.push(now);
    for (let i = 0; i < seriesCount; i++) {
      buf.series[i].push(values[i] ?? null);
    }
    if (buf.ts.length > maxPoints) {
      buf.ts = buf.ts.slice(-maxPoints);
      for (let i = 0; i < seriesCount; i++) {
        buf.series[i] = buf.series[i].slice(-maxPoints);
      }
    }
    const next = [[...buf.ts]];
    for (let i = 0; i < seriesCount; i++) {
      next.push([...buf.series[i]]);
    }
    setData(next);
  }, [seriesCount, maxPoints]);

  const clear = useCallback(() => {
    bufRef.current = { ts: [], series: Array.from({ length: seriesCount }, () => []) };
    const empty = [[]];
    for (let i = 0; i < seriesCount; i++) empty.push([]);
    setData(empty);
  }, [seriesCount]);

  return { data, push, clear };
}
