import React, { useState, useMemo } from 'react';
import { useMatrix } from '../../hooks/useMatrix';
import MatrixGrid from './MatrixGrid';
import './Matrix.css';

const ZOOM_LEVELS = [75, 100, 125];

export default function Matrix() {
  const {
    inputs,
    outputs,
    routes,
    loading,
    error,
    connected,
    toggleRoute,
    clearAll,
    mapOneToOne,
  } = useMatrix();

  const [zoomIndex, setZoomIndex] = useState(1); // default 100%
  const zoom = ZOOM_LEVELS[zoomIndex];

  const totalInputCh = useMemo(
    () => inputs.reduce((sum, g) => sum + (g.channels ? g.channels.length : 0), 0),
    [inputs]
  );
  const totalOutputCh = useMemo(
    () => outputs.reduce((sum, g) => sum + (g.channels ? g.channels.length : 0), 0),
    [outputs]
  );

  const handleClearAll = () => {
    if (window.confirm('Clear all routes? This cannot be undone.')) {
      clearAll();
    }
  };

  const zoomOut = () => setZoomIndex((i) => Math.max(0, i - 1));
  const zoomIn = () => setZoomIndex((i) => Math.min(ZOOM_LEVELS.length - 1, i + 1));

  if (loading) {
    return (
      <div className="matrix-page">
        <div className="matrix-loading">
          <div className="spinner" />
        </div>
      </div>
    );
  }

  return (
    <div className="matrix-page">
      <div className="matrix-toolbar">
        <div className="matrix-toolbar__left">
          <h1 className="matrix-toolbar__title">Patch Matrix</h1>
          {totalInputCh > 0 && totalOutputCh > 0 && (
            <span className="matrix-toolbar__badge">
              {totalInputCh} &times; {totalOutputCh}
            </span>
          )}
          {!connected && (
            <span className="matrix-toolbar__error">Connection Lost</span>
          )}
        </div>
        <div className="matrix-toolbar__right">
          <div className="btn-group">
            <button className="btn btn-danger" onClick={handleClearAll}>
              Clear All
            </button>
            <button className="btn" onClick={mapOneToOne}>
              1:1
            </button>
          </div>
          <div className="zoom-controls">
            <button
              className="btn"
              onClick={zoomOut}
              disabled={zoomIndex === 0}
              aria-label="Zoom out"
            >
              &minus;
            </button>
            <span className="zoom-controls__label">{zoom}%</span>
            <button
              className="btn"
              onClick={zoomIn}
              disabled={zoomIndex === ZOOM_LEVELS.length - 1}
              aria-label="Zoom in"
            >
              +
            </button>
          </div>
        </div>
      </div>
      <div className="matrix-wrap">
        <div
          className="matrix-zoom"
          style={{ transform: `scale(${zoom / 100})` }}
        >
          {inputs.length > 0 && outputs.length > 0 ? (
            <MatrixGrid
              inputs={inputs}
              outputs={outputs}
              routes={routes}
              onToggle={toggleRoute}
            />
          ) : (
            <div className="matrix-loading">
              <span style={{ color: 'var(--text-tertiary)', fontSize: '0.875rem' }}>
                No sources or sinks configured
              </span>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
