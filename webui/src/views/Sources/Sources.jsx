import React, { useState } from 'react';
import { api } from '../../services/api';
import { useApi } from '../../hooks/useApi';
import { usePolling } from '../../hooks/usePolling';
import StatusDot from '../../components/shared/StatusDot';
import SourceEditModal from './SourceEditModal';
import SourceInfoModal from './SourceInfoModal';
import './Sources.css';

export default function Sources() {
  const { data, loading, refresh } = useApi(() => api.getSources(), []);
  usePolling(() => refresh(), 3000);

  const [editModal, setEditModal] = useState({ open: false, source: null });
  const [infoModal, setInfoModal] = useState({ open: false, sourceId: null });

  const sources = Array.isArray(data?.sources) ? data.sources : (Array.isArray(data) ? data : []);

  const handleAdd = () => setEditModal({ open: true, source: null });
  const handleEdit = (src) => setEditModal({ open: true, source: src });
  const handleCloseEdit = () => setEditModal({ open: false, source: null });

  const handleSave = async (id, payload) => {
    try {
      await api.addSource(id, payload);
      handleCloseEdit();
      refresh();
    } catch (err) {
      alert('Failed to save source: ' + err.message);
    }
  };

  const handleDelete = async (src) => {
    if (!window.confirm(`Delete source ${src.id} "${src.name}"?`)) return;
    try {
      await api.removeSource(src.id);
      refresh();
    } catch (err) {
      alert('Failed to delete source: ' + err.message);
    }
  };

  const handleInfo = (src) => setInfoModal({ open: true, sourceId: src.id });
  const handleCloseInfo = () => setInfoModal({ open: false, sourceId: null });

  if (loading) {
    return (
      <div className="sources">
        <div className="sources__loading"><div className="spinner" /></div>
      </div>
    );
  }

  return (
    <div className="sources">
      <div className="sources__header">
        <h2 className="sources__title">Sources</h2>
        <button className="btn btn-primary" onClick={handleAdd}>+ Add Source</button>
      </div>

      <div className="sources__card">
        {sources.length === 0 ? (
          <div className="sources__empty">No sources configured. Click "+ Add Source" to create one.</div>
        ) : (
          <table className="data-table">
            <thead>
              <tr>
                <th style={{ width: 40 }}></th>
                <th>ID</th>
                <th>Name</th>
                <th>Address</th>
                <th>Channels</th>
                <th style={{ width: 110 }}>Actions</th>
              </tr>
            </thead>
            <tbody>
              {sources.map((src) => (
                <tr key={src.id}>
                  <td>
                    <div className="sources__status-cell">
                      <StatusDot status={src.enabled !== false ? 'active' : 'inactive'} />
                    </div>
                  </td>
                  <td>{src.id}</td>
                  <td style={{ color: 'var(--text-primary)', fontWeight: 500 }}>{src.name || '--'}</td>
                  <td style={{ fontFamily: "'JetBrains Mono', monospace", fontSize: '0.8125rem' }}>
                    {src.address || '--'}
                  </td>
                  <td>{src.map ? src.map.length : '--'}</td>
                  <td>
                    <div className="sources__actions">
                      {/* Edit */}
                      <button
                        className="sources__action-btn"
                        onClick={() => handleEdit(src)}
                        title="Edit source"
                      >
                        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                          <path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7" />
                          <path d="M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z" />
                        </svg>
                      </button>
                      {/* Info / SDP */}
                      <button
                        className="sources__action-btn"
                        onClick={() => handleInfo(src)}
                        title="View SDP"
                      >
                        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                          <circle cx="12" cy="12" r="10" />
                          <line x1="12" y1="16" x2="12" y2="12" />
                          <line x1="12" y1="8" x2="12.01" y2="8" />
                        </svg>
                      </button>
                      {/* Delete */}
                      <button
                        className="sources__action-btn sources__action-btn--danger"
                        onClick={() => handleDelete(src)}
                        title="Delete source"
                      >
                        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                          <polyline points="3 6 5 6 21 6" />
                          <path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2" />
                        </svg>
                      </button>
                    </div>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>

      <SourceEditModal
        isOpen={editModal.open}
        onClose={handleCloseEdit}
        source={editModal.source}
        onSave={handleSave}
      />

      <SourceInfoModal
        isOpen={infoModal.open}
        onClose={handleCloseInfo}
        sourceId={infoModal.sourceId}
      />
    </div>
  );
}
