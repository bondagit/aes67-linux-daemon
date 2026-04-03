import { useState, useEffect, useCallback } from 'react';
import { api } from '../services/api';

function routeKey(srcId, srcCh, dstId, dstCh) {
  return `${srcId}:${srcCh}-${dstId}:${dstCh}`;
}

export function useMatrix() {
  const [inputs, setInputs] = useState([]);
  const [outputs, setOutputs] = useState([]);
  const [routes, setRoutes] = useState(new Set());
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [connected, setConnected] = useState(true);

  const parseRoutes = useCallback((routeArray) => {
    const set = new Set();
    if (Array.isArray(routeArray)) {
      routeArray.forEach((r) => {
        // API returns routes as { src: [streamId, chIdx], dst: [streamId, chIdx] }
        const srcId = Array.isArray(r.src) ? r.src[0] : r.srcId;
        const srcCh = Array.isArray(r.src) ? r.src[1] : r.srcCh;
        const dstId = Array.isArray(r.dst) ? r.dst[0] : r.dstId;
        const dstCh = Array.isArray(r.dst) ? r.dst[1] : r.dstCh;
        set.add(routeKey(srcId, srcCh, dstId, dstCh));
      });
    }
    return set;
  }, []);

  const refresh = useCallback(async () => {
    try {
      setError(null);
      const data = await api.getMatrix();
      setInputs(data.inputs || []);
      setOutputs(data.outputs || []);
      setRoutes(parseRoutes(data.routes));
      setConnected(true);
    } catch (err) {
      setError(err.message);
      setConnected(false);
    } finally {
      setLoading(false);
    }
  }, [parseRoutes]);

  useEffect(() => {
    refresh();
  }, [refresh]);

  const toggleRoute = useCallback(async (src, dst) => {
    const key = routeKey(src.id, src.ch, dst.id, dst.ch);
    const wasActive = routes.has(key);
    const action = wasActive ? 'disconnect' : 'connect';

    // Optimistic update
    setRoutes((prev) => {
      const next = new Set(prev);
      if (wasActive) {
        next.delete(key);
      } else {
        next.add(key);
      }
      return next;
    });

    try {
      await api.setMatrixRoute({
        src_stream: src.id,
        src_channel: src.ch,
        dst_stream: dst.id,
        dst_channel: dst.ch,
        action,
      });
    } catch (err) {
      // Revert on failure
      setRoutes((prev) => {
        const next = new Set(prev);
        if (wasActive) {
          next.add(key);
        } else {
          next.delete(key);
        }
        return next;
      });
      setError(`Failed to ${action}: ${err.message}`);
    }
  }, [routes]);

  const clearAll = useCallback(async () => {
    try {
      await api.setMatrix({ routes: [] });
      await refresh();
    } catch (err) {
      setError(`Failed to clear: ${err.message}`);
    }
  }, [refresh]);

  const mapOneToOne = useCallback(async () => {
    const newRoutes = [];
    const maxGroups = Math.min(inputs.length, outputs.length);
    for (let g = 0; g < maxGroups; g++) {
      const inp = inputs[g];
      const out = outputs[g];
      const maxCh = Math.min(
        inp.channels ? inp.channels.length : 0,
        out.channels ? out.channels.length : 0
      );
      for (let c = 0; c < maxCh; c++) {
        newRoutes.push({
          src: [inp.id, c],
          dst: [out.id, c],
        });
      }
    }
    try {
      await api.setMatrix({ routes: newRoutes });
      await refresh();
    } catch (err) {
      setError(`Failed to set 1:1: ${err.message}`);
    }
  }, [inputs, outputs, refresh]);

  return {
    inputs,
    outputs,
    routes,
    loading,
    error,
    connected,
    toggleRoute,
    clearAll,
    mapOneToOne,
    refresh,
  };
}
