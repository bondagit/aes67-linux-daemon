import React, { useState, useEffect } from 'react';
import Modal from '../../components/shared/Modal';
import { api } from '../../services/api';

export default function SourceInfoModal({ isOpen, onClose, sourceId }) {
  const [sdp, setSdp] = useState('');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState(null);
  const [copied, setCopied] = useState(false);

  useEffect(() => {
    if (!isOpen || sourceId == null) return;
    setLoading(true);
    setError(null);
    setSdp('');
    setCopied(false);
    api.getSourceSDP(sourceId)
      .then((text) => setSdp(text))
      .catch((err) => setError(err.message))
      .finally(() => setLoading(false));
  }, [isOpen, sourceId]);

  const handleCopy = () => {
    navigator.clipboard.writeText(sdp).then(() => {
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    });
  };

  return (
    <Modal isOpen={isOpen} onClose={onClose} title={`Source ${sourceId} — SDP`} width="600px">
      {loading && (
        <div className="source-info__loading">
          <div className="spinner" />
        </div>
      )}
      {error && <div className="source-info__error">{error}</div>}
      {!loading && !error && (
        <>
          <pre className="source-info__sdp">{sdp || 'No SDP data available.'}</pre>
          <div className="source-info__actions">
            <button className="btn" onClick={handleCopy} disabled={!sdp}>
              <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                <rect x="9" y="9" width="13" height="13" rx="2" ry="2" />
                <path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1" />
              </svg>
              {copied ? 'Copied' : 'Copy SDP'}
            </button>
          </div>
        </>
      )}
    </Modal>
  );
}
