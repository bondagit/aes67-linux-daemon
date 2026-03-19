import React from 'react';
import { useNavigate } from 'react-router-dom';
import { api } from '../../services/api';
import { useApi } from '../../hooks/useApi';
import { usePolling } from '../../hooks/usePolling';
import StatusCard from '../../components/dashboard/StatusCard';
import StatusDot from '../../components/shared/StatusDot';
import Badge from '../../components/shared/Badge';
import './Dashboard.css';

function ptpStatusText(status) {
  if (!status) return 'UNKNOWN';
  const s = String(status.status || '').toLowerCase();
  if (s === 'locked' || s === '2') return 'LOCKED';
  if (s === 'locking' || s === '1') return 'UNLOCKING';
  return 'UNLOCKED';
}

function formatGmid(status) {
  if (!status || !status.gmid) return '--';
  return status.gmid;
}

function formatJitter(status) {
  if (!status || status.jitter == null) return '-- \u03BCs';
  return `${status.jitter} \u03BCs`;
}

function dscpLabel(dscp) {
  const map = { 0: 'BE (0)', 8: 'CS1 (8)', 16: 'CS2 (16)', 24: 'CS3 (24)', 32: 'CS4 (32)', 40: 'CS5 (40)', 46: 'EF (46)', 48: 'CS6 (48)', 56: 'CS7 (56)' };
  return map[dscp] || String(dscp ?? '--');
}

export default function Dashboard() {
  const navigate = useNavigate();

  const config = useApi(() => api.getConfig(), []);
  const version = useApi(() => api.getVersion(), []);
  const ptpStatus = useApi(() => api.getPTPStatus(), []);
  const ptpConfig = useApi(() => api.getPTPConfig(), []);
  const sources = useApi(() => api.getSources(), []);
  const sinks = useApi(() => api.getSinks(), []);
  const remoteSources = useApi(() => api.getBrowseSources(), []);

  // Fast polling for PTP status
  usePolling(() => ptpStatus.refresh(), 2000);
  // Slower polling for everything else
  usePolling(() => {
    config.refresh();
    sources.refresh();
    sinks.refresh();
    ptpConfig.refresh();
    remoteSources.refresh();
  }, 5000);

  const cfg = config.data || {};
  const ptp = ptpStatus.data || {};
  const ptpCfg = ptpConfig.data || {};
  const srcList = Array.isArray(sources.data?.sources) ? sources.data.sources : (Array.isArray(sources.data) ? sources.data : []);
  const snkList = Array.isArray(sinks.data?.sinks) ? sinks.data.sinks : (Array.isArray(sinks.data) ? sinks.data : []);
  const remoteList = Array.isArray(remoteSources.data?.remote_sources) ? remoteSources.data.remote_sources : [];

  const isLoading = config.loading && ptpStatus.loading;

  if (isLoading) {
    return (
      <div className="dashboard">
        <div className="dashboard__loading">
          <div className="spinner" />
        </div>
      </div>
    );
  }

  const locked = ptpStatusText(ptp) === 'LOCKED';
  const sampleRate = cfg.sample_rate || 48000;
  const ticSize = cfg.tic_frame_size_at_1fs || '--';
  const playoutDelay = cfg.playout_delay || '--';
  const maxTic = cfg.max_tic_frame_size || '--';
  const iface = cfg.interface_name || cfg.ip_addr || '--';
  const ip = cfg.ip_addr || '--';
  const mdns = cfg.mdns_enabled !== false ? 'on' : 'off';
  const rtpMcast = cfg.rtp_mcast_base || '--';
  const rtpPort = cfg.rtp_port || '--';
  const rtspPort = cfg.rtsp_port || '--';
  const httpEnabled = cfg.streamer_enabled === true;
  const httpChannels = cfg.streamer_channels || '--';
  const httpBuffer = cfg.streamer_player_buffer_files_num || '--';
  const httpPort = cfg.http_port || '--';
  const sapEnabled = cfg.sap_mcast_addr || cfg.sap_interval;
  const activeSources = srcList.filter(s => s.enabled !== false).length;
  const activeSinks = snkList.filter(s => s.enabled !== false).length;

  return (
    <div className="dashboard">
      {/* Top row — Status Cards */}
      <div className="dashboard__status-row">
        <StatusCard
          title="PTP Clock"
          value={ptpStatusText(ptp)}
          subtitle={`Grandmaster \u00B7 Domain ${ptpCfg.domain ?? '--'}`}
          variant={locked ? 'success' : 'default'}
          indicator={locked ? { color: 'var(--route-on)', glow: 'var(--route-glow)', pulse: true } : undefined}
          details={[
            { label: 'GMID', value: formatGmid(ptp) },
            { label: 'JITTER', value: formatJitter(ptp) },
            { label: 'DSCP', value: dscpLabel(ptpCfg.dscp) },
          ]}
        />
        <StatusCard
          title="Audio Engine"
          value={`${(sampleRate / 1000)} kHz`}
          subtitle={`TIC frame ${ticSize} samples`}
          details={[
            { label: 'TIC SIZE', value: String(ticSize) },
            { label: 'PLAYOUT', value: `${playoutDelay}ms` },
            { label: 'MAX TIC', value: String(maxTic) },
          ]}
        />
        <StatusCard
          title="Network"
          value={iface}
          subtitle={`${ip} \u00B7 mDNS ${mdns}`}
          details={[
            { label: 'MCAST', value: rtpMcast },
            { label: 'RTP', value: `:${rtpPort}` },
            { label: 'RTSP', value: `:${rtspPort}` },
          ]}
        />
      </div>

      {/* Middle row — Sources & Sinks */}
      <div className="dashboard__panels-row">
        {/* Sources */}
        <div className="dashboard-panel">
          <div className="dashboard-panel__header">
            <span className="dashboard-panel__title">Sources</span>
            <Badge variant="success">{activeSources} active</Badge>
            <span className="dashboard-panel__spacer" />
            <button className="dashboard-panel__add" onClick={() => navigate('/sources')}>+ Add</button>
          </div>
          <div className="dashboard-panel__body">
            {srcList.length === 0 ? (
              <div className="dashboard-panel__empty">No sources configured</div>
            ) : (
              srcList.map((src, i) => (
                <div key={src.id ?? i} className="stream-row">
                  <StatusDot status={src.enabled !== false ? 'active' : 'inactive'} />
                  <div className="stream-row__info">
                    <div className="stream-row__name">{src.name || `Source ${src.id ?? i}`}</div>
                    <div className="stream-row__subtitle">
                      {src.address || ''}{src.map ? ` \u00B7 ${src.map.length} channels` : ''}
                    </div>
                  </div>
                  <div className="stream-row__badges">
                    {src.enabled !== false && <Badge variant="success">LIVE</Badge>}
                  </div>
                </div>
              ))
            )}
          </div>
        </div>

        {/* Sinks */}
        <div className="dashboard-panel">
          <div className="dashboard-panel__header">
            <span className="dashboard-panel__title">Sinks</span>
            <Badge variant="info">{activeSinks} active</Badge>
            <span className="dashboard-panel__spacer" />
            <button className="dashboard-panel__add" onClick={() => navigate('/sinks')}>+ Add</button>
          </div>
          <div className="dashboard-panel__body">
            {snkList.length === 0 ? (
              <div className="dashboard-panel__empty">No sinks configured</div>
            ) : (
              snkList.map((snk, i) => {
                const hasError = false; // sink status requires separate API call per sink
                return (
                  <div key={snk.id ?? i} className="stream-row">
                    <StatusDot status={snk.enabled !== false ? 'active' : 'inactive'} />
                    <div className="stream-row__info">
                      <div className="stream-row__name">{snk.name || `Sink ${snk.id ?? i}`}</div>
                      <div className="stream-row__subtitle">
                        {snk.map ? `${snk.map.length} channels` : ''}{snk.io ? ` \u00B7 ${snk.io}` : ''}
                      </div>
                    </div>
                    <div className="stream-row__badges">
                      {snk.error_count > 0 && <Badge variant="error">{snk.error_count} err</Badge>}
                      <Badge variant={hasError ? 'error' : 'success'}>{hasError ? 'ERR' : 'OK'}</Badge>
                    </div>
                  </div>
                );
              })
            )}
          </div>
        </div>
      </div>

      {/* Bottom row — Streamer + Network Discovery */}
      <div className="dashboard__bottom-row">
        <div className="dashboard-compact">
          <div className="dashboard-compact__info">
            <div className="dashboard-compact__title">HTTP Streamer</div>
            <div className="dashboard-compact__text">
              {httpEnabled
                ? `${httpChannels} channels \u00B7 ${httpBuffer}s buffer \u00B7 Port ${httpPort}`
                : 'Disabled'}
            </div>
          </div>
          <Badge variant={httpEnabled ? 'success' : 'neutral'}>{httpEnabled ? 'Enabled' : 'Disabled'}</Badge>
        </div>
      </div>

      {/* Network Discovery — full panel */}
      <div className="dashboard-panel dashboard-panel--discovery">
        <div className="dashboard-panel__header">
          <span className="dashboard-panel__title">Network Discovery</span>
          <Badge variant={remoteList.length > 0 ? 'success' : 'neutral'}>{remoteList.length} found</Badge>
          <span className="dashboard-panel__spacer" />
          <button className="dashboard-panel__add" onClick={() => navigate('/browser')}>Browse All</button>
        </div>
        <div className="dashboard-panel__body">
          {remoteList.length === 0 ? (
            <div className="dashboard-panel__empty">No remote sources discovered on the network</div>
          ) : (
            remoteList.map((rs, i) => (
              <div key={rs.id || i} className="stream-row">
                <StatusDot status="active" />
                <div className="stream-row__info">
                  <div className="stream-row__name">{rs.name || rs.id || `Remote ${i}`}</div>
                  <div className="stream-row__subtitle">
                    {rs.address || '--'}{rs.domain ? ` \u00B7 ${rs.domain}` : ''}
                    {rs.last_seen != null ? ` \u00B7 ${rs.last_seen}s ago` : ''}
                  </div>
                </div>
                <div className="stream-row__badges">
                  <Badge variant={rs.source === 'SAP' ? 'info' : 'success'}>
                    {rs.source || 'mDNS'}
                  </Badge>
                  <button
                    className="stream-row__action-btn"
                    title="Add as sink using this source's SDP"
                    onClick={() => navigate('/sinks', { state: { addFromSDP: rs.sdp, name: rs.name } })}
                  >
                    + Sink
                  </button>
                </div>
              </div>
            ))
          )}
        </div>
      </div>
    </div>
  );
}
