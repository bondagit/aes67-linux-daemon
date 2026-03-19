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
        set.add(routeKey(r.srcId, r.srcCh, r.dstId, r.dstCh));
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
      await api.setMatrixRoute({ src, dst, action });
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
          srcId: inp.id,
          srcCh: inp.channels[c].id !== undefined ? inp.channels[c].id : c,
          dstId: out.id,
          dstCh: out.channels[c].id !== undefined ? out.channels[c].id : c,
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
