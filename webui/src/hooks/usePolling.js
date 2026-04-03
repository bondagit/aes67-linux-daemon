import { useEffect, useRef } from 'react';

export function usePolling(callback, intervalMs) {
  const savedCallback = useRef(callback);

  useEffect(() => { savedCallback.current = callback; }, [callback]);

  useEffect(() => {
    if (!intervalMs) return;
    const tick = () => savedCallback.current();
    const id = setInterval(tick, intervalMs);
    const onVisibility = () => { if (document.hidden) clearInterval(id); };
    document.addEventListener('visibilitychange', onVisibility);
    return () => {
      clearInterval(id);
      document.removeEventListener('visibilitychange', onVisibility);
    };
  }, [intervalMs]);
}
