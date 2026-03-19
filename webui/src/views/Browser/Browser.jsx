import React, { useState } from 'react';
import { api } from '../../services/api';
import { useApi } from '../../hooks/useApi';
import { usePolling } from '../../hooks/usePolling';
import Badge from '../../components/shared/Badge';
import Modal from '../../components/shared/Modal';
import './Browser.css';

function formatRelativeTime(seconds) {
  if (seconds == null) return 'N/A';
  if (seconds < 60) return `${seconds}s ago`;
  if (seconds < 3600) return `${Math.floor(seconds / 60)}m ago`;
  if (seconds < 86400) return `${Math.floor(seconds / 3600)}h ago`;
  return `${Math.floor(seconds / 86400)}d ago`;
}

export default function Browser() {
  const { data, loading, refresh } = useApi(() => api.getBrowseSources(), []);
  usePolling(refresh, 10000);

  const [selected, setSelected] = useState(null);

  const sources = data?.remote_sources || [];

  if (loading && !data) {
    return (
      <div className="browser">
        <div className="browser__loading">Loading remote sources...</div>
      </div>
    );
  }

  return (
    <div className="browser">
      <div className="browser__header">
        <h2 className="browser__title">Network Browser</h2>
        <div className="browser__refresh-indicator">
          <span className="browser__refresh-dot" />
          Auto-refresh 10s
        </div>
      </div>

      <div className="browser__table-wrap">
        {sources.length === 0 ? (
          <div className="browser__empty">No remote sources discovered.</div>
        ) : (
          <table className="browser__table">
            <thead>
              <tr>
                <th>Type</th>
                <th>Source Name</th>
                <th>Domain</th>
                <th>Address</th>
                <th>Last Seen</th>
                <th>Actions</th>
              </tr>
            </thead>
            <tbody>
              {sources.map((src) => (
                <tr key={src.id}>
                  <td>
                    {src.source === 'SAP' ? (
                      <Badge variant="info">SAP</Badge>
                    ) : (
                      <span className="browser__badge-mdns" style={{
                        display: 'inline-block',
                        padding: '2px 8px',
                        borderRadius: '4px',
                        fontSize: '10px',
                        fontWeight: 600,
                        letterSpacing: '0.03em',
                        textTransform: 'uppercase',
                        lineHeight: '16px',
                        whiteSpace: 'nowrap',
                      }}>mDNS</span>
                    )}
                  </td>
                  <td className="browser__td-name">{src.name}</td>
                  <td>{src.source === 'mDNS' ? src.domain : 'N/A'}</td>
                  <td>{src.address}</td>
                  <td>{formatRelativeTime(src.last_seen)}</td>
                  <td>
                    <button
                      className="browser__details-btn"
                      onClick={() => setSelected(src)}
                    >
                      Details
                    </button>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>

      <Modal
        isOpen={!!selected}
        onClose={() => setSelected(null)}
        title="Remote Source Details"
        width="640px"
      >
        {selected && (
          <>
            <div className="browser__detail-grid">
              <span className="browser__detail-label">ID</span>
              <span className="browser__detail-value">{selected.id}</span>

              <span className="browser__detail-label">Type</span>
              <span className="browser__detail-value">{selected.source}</span>

              <span className="browser__detail-label">Name</span>
              <span className="browser__detail-value">{selected.name}</span>

              <span className="browser__detail-label">Domain</span>
              <span className="browser__detail-value">
                {selected.source === 'mDNS' ? selected.domain : 'N/A'}
              </span>

              <span className="browser__detail-label">Address</span>
              <span className="browser__detail-value">{selected.address}</span>

              <span className="browser__detail-label">Last Seen</span>
              <span className="browser__detail-value">
                {formatRelativeTime(selected.last_seen)}
              </span>

              <span className="browser__detail-label">Announce Period</span>
              <span className="browser__detail-value">
                {selected.source === 'SAP' ? `${selected.announce_period}s` : 'N/A'}
              </span>
            </div>

            <div className="browser__detail-label" style={{ marginBottom: '0.5rem' }}>
              SDP Content
            </div>
            <pre className="browser__sdp-block">{selected.sdp}</pre>
          </>
        )}
      </Modal>
    </div>
  );
}
