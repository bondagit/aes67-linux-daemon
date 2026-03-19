import React, { useState, useCallback } from 'react';
import { api } from '../../services/api';
import { useApi } from '../../hooks/useApi';
import { usePolling } from '../../hooks/usePolling';
import StatusDot from '../../components/shared/StatusDot';
import Badge from '../../components/shared/Badge';
import SinkEditModal from './SinkEditModal';
import './Sinks.css';

export default function Sinks() {
  const { data: sinks, loading, refresh } = useApi(() => api.getSinks(), []);
  const [statuses, setStatuses] = useState({});
  const [modalOpen, setModalOpen] = useState(false);
  const [editSink, setEditSink] = useState(null);

  const fetchStatuses = useCallback(async () => {
    const list = sinks && sinks.sinks ? sinks.sinks : [];
    if (list.length === 0) return;
    const results = {};
    await Promise.allSettled(
      list.map(async (s) => {
        try {
          const st = await api.getSinkStatus(s.id);
          results[s.id] = st;
        } catch {
          results[s.id] = null;
        }
      })
    );
    setStatuses(results);
  }, [sinks]);

  usePolling(refresh, 3000);
  usePolling(fetchStatuses, 2000);

  const handleDelete = async (sink) => {
    if (!window.confirm(`Delete sink "${sink.name}" (ID ${sink.id})?`)) return;
    try {
      await api.removeSink(sink.id);
      refresh();
    } catch (err) {
      console.error('Failed to delete sink:', err);
    }
  };

  const handleEdit = (sink) => {
    setEditSink(sink);
    setModalOpen(true);
  };

  const handleAdd = () => {
    setEditSink(null);
    setModalOpen(true);
  };

  const handleSave = () => {
    setModalOpen(false);
    setEditSink(null);
    refresh();
  };

  const sinkList = sinks && sinks.sinks ? sinks.sinks : [];

  return (
    <div className="sinks-view">
      <div className="sinks-header">
        <h2 className="sinks-title">Sinks</h2>
        <button className="sinks-add-btn" onClick={handleAdd}>+ Add Sink</button>
      </div>

      <div className="sinks-table-wrap">
        {loading ? (
          <div className="sinks-loading">Loading sinks...</div>
        ) : sinkList.length === 0 ? (
          <div className="sinks-empty">No sinks configured.</div>
        ) : (
          <table className="sinks-table">
            <thead>
              <tr>
                <th></th>
                <th>ID</th>
                <th>Name</th>
                <th>Channels</th>
                <th>Status</th>
                <th>Errors</th>
                <th>Min Arrival</th>
                <th>Actions</th>
              </tr>
            </thead>
            <tbody>
              {sinkList.map((sink) => {
                const st = statuses[sink.id];
                const isActive = st && st.is_rtp_seq_id_error !== undefined;
                const hasError = st && (st.is_rtp_seq_id_error || st.is_rtp_ssrc_error || st.is_rtp_payload_type_error || st.is_rtp_sac_error);
                const minTime = st && st.min_time !== undefined ? st.min_time : '-';
                const errCount =
                  st
                    ? (st.rtp_seq_id_error_count || 0) +
                      (st.rtp_ssrc_error_count || 0) +
                      (st.rtp_payload_type_error_count || 0) +
                      (st.rtp_sac_error_count || 0)
                    : '-';

                return (
                  <tr key={sink.id}>
                    <td>
                      <StatusDot status={isActive ? 'active' : 'inactive'} />
                    </td>
                    <td>{sink.id}</td>
                    <td style={{ color: 'var(--text-primary)' }}>{sink.name}</td>
                    <td>{sink.map ? sink.map.length : '-'}</td>
                    <td>
                      {st ? (
                        hasError ? <Badge variant="error">ERR</Badge> : <Badge variant="success">OK</Badge>
                      ) : (
                        <Badge variant="neutral">--</Badge>
                      )}
                    </td>
                    <td>{errCount}</td>
                    <td>{minTime}</td>
                    <td>
                      <div className="sinks-actions">
                        <button className="sinks-action-btn" onClick={() => handleEdit(sink)}>Edit</button>
                        <button className="sinks-action-btn delete" onClick={() => handleDelete(sink)}>Delete</button>
                      </div>
                    </td>
                  </tr>
                );
              })}
            </tbody>
          </table>
        )}
      </div>

      <SinkEditModal
        isOpen={modalOpen}
        onClose={() => { setModalOpen(false); setEditSink(null); }}
        sink={editSink}
        onSave={handleSave}
      />
    </div>
  );
}
