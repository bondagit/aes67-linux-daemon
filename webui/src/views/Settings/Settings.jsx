import React, { useState } from 'react';
import { toast } from 'react-toastify';
import { api } from '../../services/api';
import { useApi } from '../../hooks/useApi';
import './Settings.css';

/* ── Collapsible Section ─────────────────────────────────── */

function Section({ title, defaultOpen = false, children }) {
  const [open, setOpen] = useState(defaultOpen);
  return (
    <div className="settings-section">
      <div className="settings-section__header" onClick={() => setOpen(o => !o)}>
        <span className={`settings-section__chevron ${open ? 'settings-section__chevron--open' : ''}`}>&#9654;</span>
        <span className="settings-section__title">{title}</span>
      </div>
      {open && <div className="settings-section__body">{children}</div>}
    </div>
  );
}

/* ── DSCP options ────────────────────────────────────────── */

const dscpOptions = [
  { value: 0, label: 'CS0 (0)' },
  { value: 8, label: 'CS1 (8)' },
  { value: 10, label: 'AF11 (10)' },
  { value: 12, label: 'AF12 (12)' },
  { value: 14, label: 'AF13 (14)' },
  { value: 16, label: 'CS2 (16)' },
  { value: 18, label: 'AF21 (18)' },
  { value: 20, label: 'AF22 (20)' },
  { value: 22, label: 'AF23 (22)' },
  { value: 24, label: 'CS3 (24)' },
  { value: 26, label: 'AF31 (26)' },
  { value: 28, label: 'AF32 (28)' },
  { value: 30, label: 'AF33 (30)' },
  { value: 32, label: 'CS4 (32)' },
  { value: 34, label: 'AF41 (34)' },
  { value: 36, label: 'AF42 (36)' },
  { value: 38, label: 'AF43 (38)' },
  { value: 40, label: 'CS5 (40)' },
  { value: 46, label: 'EF (46)' },
  { value: 48, label: 'CS6 (48)' },
  { value: 56, label: 'CS7 (56)' },
];

/* ── Main Component ──────────────────────────────────────── */

export default function Settings() {
  const config = useApi(() => api.getConfig(), []);
  const ptpConfig = useApi(() => api.getPTPConfig(), []);
  const version = useApi(() => api.getVersion(), []);

  /* Local form state — initialized from fetched data */
  const [form, setForm] = useState({});
  const [ptpForm, setPtpForm] = useState({});
  const [initialized, setInitialized] = useState(false);
  const [ptpInitialized, setPtpInitialized] = useState(false);

  const cfg = config.data || {};
  const ptpCfg = ptpConfig.data || {};
  const ver = version.data || {};

  // Initialize form state once data arrives
  if (config.data && !initialized) {
    setForm({
      sample_rate: cfg.sample_rate,
      playout_delay: cfg.playout_delay,
      tic_frame_size_at_1fs: cfg.tic_frame_size_at_1fs,
      max_tic_frame_size: cfg.max_tic_frame_size,
      rtsp_port: cfg.rtsp_port,
      rtp_mcast_base: cfg.rtp_mcast_base,
      rtp_port: cfg.rtp_port,
      custom_node_id: cfg.custom_node_id || '',
      mdns_enabled: cfg.mdns_enabled,
      streamer_enabled: cfg.streamer_enabled,
      streamer_channels: cfg.streamer_channels,
      streamer_files_num: cfg.streamer_files_num,
      streamer_file_duration: cfg.streamer_file_duration,
      streamer_player_buffer_files_num: cfg.streamer_player_buffer_files_num,
      log_severity: cfg.log_severity,
      syslog_proto: cfg.syslog_proto,
      syslog_server: cfg.syslog_server || '',
      sap_mcast_addr: cfg.sap_mcast_addr,
      sap_interval: cfg.sap_interval,
      auto_sinks_update: cfg.auto_sinks_update,
      status_file: cfg.status_file || '',
    });
    setInitialized(true);
  }

  if (ptpConfig.data && !ptpInitialized) {
    setPtpForm({
      domain: ptpCfg.domain,
      dscp: ptpCfg.dscp,
    });
    setPtpInitialized(true);
  }

  const updateForm = (field, value) => setForm(prev => ({ ...prev, [field]: value }));
  const updatePtp = (field, value) => setPtpForm(prev => ({ ...prev, [field]: value }));

  /* ── Save helpers ──────────────────────────────────────── */

  async function saveConfig(fields) {
    try {
      const payload = {};
      for (const key of fields) {
        payload[key] = form[key];
      }
      await api.setConfig(payload);
      toast.success('Settings saved');
    } catch (err) {
      toast.error('Failed to save settings: ' + err.message);
    }
  }

  async function savePtp() {
    try {
      await api.setPTPConfig({ domain: Number(ptpForm.domain), dscp: Number(ptpForm.dscp) });
      toast.success('Settings saved');
    } catch (err) {
      toast.error('Failed to save PTP settings: ' + err.message);
    }
  }

  /* ── Loading ───────────────────────────────────────────── */

  const isLoading = config.loading || ptpConfig.loading || version.loading;

  if (isLoading) {
    return (
      <div className="settings">
        <div className="settings__loading">
          <div className="spinner" />
        </div>
      </div>
    );
  }

  /* ── Render ────────────────────────────────────────────── */

  return (
    <div className="settings">
      <h2 className="settings__title">Settings</h2>

      {/* 1. System Info — always expanded, read-only */}
      <Section title="System Info" defaultOpen={true}>
        <div className="settings-grid">
          <span className="settings-grid__label">Version</span>
          <span className="settings-grid__value">{ver.version || '--'}</span>

          <span className="settings-grid__label">Node ID</span>
          <span className="settings-grid__value">{cfg.node_id || '--'}</span>

          <span className="settings-grid__label">IP Address</span>
          <span className="settings-grid__value">{cfg.ip_addr || '--'}</span>

          <span className="settings-grid__label">MAC Address</span>
          <span className="settings-grid__value">{cfg.mac_addr || '--'}</span>

          <span className="settings-grid__label">Interface</span>
          <span className="settings-grid__value">{cfg.interface_name || '--'}</span>
        </div>
      </Section>

      {/* 2. PTP Configuration */}
      <Section title="PTP Configuration">
        <div className="settings-grid">
          <label className="settings-grid__label">Domain</label>
          <input
            type="number"
            min="0"
            max="127"
            value={ptpForm.domain ?? ''}
            onChange={e => updatePtp('domain', e.target.value)}
          />

          <label className="settings-grid__label">DSCP</label>
          <select
            value={ptpForm.dscp ?? ''}
            onChange={e => updatePtp('dscp', e.target.value)}
          >
            {dscpOptions.map(o => (
              <option key={o.value} value={o.value}>{o.label}</option>
            ))}
          </select>
        </div>
        <div className="settings-section__actions">
          <button className="btn btn-primary" onClick={savePtp}>Save PTP</button>
        </div>
      </Section>

      {/* 3. Audio */}
      <Section title="Audio">
        <div className="settings-grid">
          <label className="settings-grid__label">Sample Rate</label>
          <select
            value={form.sample_rate ?? ''}
            onChange={e => updateForm('sample_rate', Number(e.target.value))}
          >
            <option value={44100}>44.1 kHz</option>
            <option value={48000}>48 kHz</option>
            <option value={96000}>96 kHz</option>
            <option value={192000}>192 kHz</option>
            <option value={384000}>384 kHz</option>
          </select>

          <label className="settings-grid__label">Playout Delay (samples)</label>
          <input
            type="number"
            min="0"
            max="4000"
            value={form.playout_delay ?? ''}
            onChange={e => updateForm('playout_delay', Number(e.target.value))}
          />

          <label className="settings-grid__label">TIC Frame Size @1FS</label>
          <select
            value={form.tic_frame_size_at_1fs ?? ''}
            onChange={e => updateForm('tic_frame_size_at_1fs', Number(e.target.value))}
          >
            <option value={48}>48 - 1ms</option>
            <option value={64}>64 - 1.3ms</option>
            <option value={96}>96 - 2ms</option>
            <option value={128}>128 - 2.7ms</option>
            <option value={192}>192 - 4ms</option>
          </select>

          <label className="settings-grid__label">Max TIC Frame Size</label>
          <input
            type="number"
            min="192"
            max="8192"
            value={form.max_tic_frame_size ?? ''}
            onChange={e => updateForm('max_tic_frame_size', Number(e.target.value))}
          />
        </div>
        <div className="settings-section__actions">
          <button
            className="btn btn-primary"
            onClick={() => saveConfig(['sample_rate', 'playout_delay', 'tic_frame_size_at_1fs', 'max_tic_frame_size'])}
          >
            Save Audio
          </button>
        </div>
      </Section>

      {/* 4. Network */}
      <Section title="Network">
        <div className="settings-grid">
          <span className="settings-grid__label">Interface</span>
          <span className="settings-grid__value">{cfg.interface_name || '--'}</span>

          <span className="settings-grid__label">HTTP Port</span>
          <span className="settings-grid__value">{cfg.http_port || '--'}</span>

          <label className="settings-grid__label">RTSP Port</label>
          <input
            type="number"
            min="1024"
            max="65535"
            value={form.rtsp_port ?? ''}
            onChange={e => updateForm('rtsp_port', Number(e.target.value))}
          />

          <label className="settings-grid__label">RTP Multicast Base</label>
          <input
            type="text"
            value={form.rtp_mcast_base ?? ''}
            onChange={e => updateForm('rtp_mcast_base', e.target.value)}
          />

          <label className="settings-grid__label">RTP Port</label>
          <input
            type="number"
            min="1024"
            max="65535"
            value={form.rtp_port ?? ''}
            onChange={e => updateForm('rtp_port', Number(e.target.value))}
          />

          <label className="settings-grid__label">Custom Node ID</label>
          <input
            type="text"
            value={form.custom_node_id ?? ''}
            onChange={e => updateForm('custom_node_id', e.target.value)}
          />

          <label className="settings-grid__label">mDNS Enabled</label>
          <input
            type="checkbox"
            checked={!!form.mdns_enabled}
            onChange={e => updateForm('mdns_enabled', e.target.checked)}
          />
        </div>
        <div className="settings-section__actions">
          <button
            className="btn btn-primary"
            onClick={() => saveConfig(['rtsp_port', 'rtp_mcast_base', 'rtp_port', 'custom_node_id', 'mdns_enabled'])}
          >
            Save Network
          </button>
        </div>
      </Section>

      {/* 5. Streamer */}
      <Section title="Streamer">
        <div className="settings-grid">
          <label className="settings-grid__label">Enabled</label>
          <input
            type="checkbox"
            checked={!!form.streamer_enabled}
            onChange={e => updateForm('streamer_enabled', e.target.checked)}
          />

          <label className="settings-grid__label">Channels</label>
          <input
            type="number"
            min="2"
            max="16"
            value={form.streamer_channels ?? ''}
            onChange={e => updateForm('streamer_channels', Number(e.target.value))}
          />

          <label className="settings-grid__label">File Count</label>
          <input
            type="number"
            min="4"
            max="16"
            value={form.streamer_files_num ?? ''}
            onChange={e => updateForm('streamer_files_num', Number(e.target.value))}
          />

          <label className="settings-grid__label">File Duration (sec)</label>
          <input
            type="number"
            min="1"
            max="4"
            value={form.streamer_file_duration ?? ''}
            onChange={e => updateForm('streamer_file_duration', Number(e.target.value))}
          />

          <label className="settings-grid__label">Buffer Files</label>
          <input
            type="number"
            min="1"
            max="2"
            value={form.streamer_player_buffer_files_num ?? ''}
            onChange={e => updateForm('streamer_player_buffer_files_num', Number(e.target.value))}
          />
        </div>
        <div className="settings-section__actions">
          <button
            className="btn btn-primary"
            onClick={() => saveConfig(['streamer_enabled', 'streamer_channels', 'streamer_files_num', 'streamer_file_duration', 'streamer_player_buffer_files_num'])}
          >
            Save Streamer
          </button>
        </div>
      </Section>

      {/* 6. Logging */}
      <Section title="Logging">
        <div className="settings-grid">
          <label className="settings-grid__label">Severity</label>
          <select
            value={form.log_severity ?? ''}
            onChange={e => updateForm('log_severity', Number(e.target.value))}
          >
            <option value={0}>0 - Emergency</option>
            <option value={1}>1 - Debug</option>
            <option value={2}>2 - Info</option>
            <option value={3}>3 - Warning</option>
            <option value={4}>4 - Error</option>
            <option value={5}>5 - Fatal</option>
          </select>

          <label className="settings-grid__label">Syslog Protocol</label>
          <select
            value={form.syslog_proto ?? ''}
            onChange={e => updateForm('syslog_proto', e.target.value)}
          >
            <option value="none">None</option>
            <option value="udp">UDP</option>
            <option value="tcp">TCP</option>
          </select>

          <label className="settings-grid__label">Syslog Server</label>
          <input
            type="text"
            value={form.syslog_server ?? ''}
            onChange={e => updateForm('syslog_server', e.target.value)}
            disabled={form.syslog_proto === 'none'}
          />
        </div>
        <div className="settings-section__actions">
          <button
            className="btn btn-primary"
            onClick={() => saveConfig(['log_severity', 'syslog_proto', 'syslog_server'])}
          >
            Save Logging
          </button>
        </div>
      </Section>

      {/* 7. SAP */}
      <Section title="SAP">
        <div className="settings-grid">
          <label className="settings-grid__label">Multicast Address</label>
          <input
            type="text"
            value={form.sap_mcast_addr ?? ''}
            onChange={e => updateForm('sap_mcast_addr', e.target.value)}
          />

          <label className="settings-grid__label">Interval (sec)</label>
          <input
            type="number"
            min="0"
            max="255"
            value={form.sap_interval ?? ''}
            onChange={e => updateForm('sap_interval', Number(e.target.value))}
          />
        </div>
        <div className="settings-section__actions">
          <button
            className="btn btn-primary"
            onClick={() => saveConfig(['sap_mcast_addr', 'sap_interval'])}
          >
            Save SAP
          </button>
        </div>
      </Section>

      {/* 8. Advanced */}
      <Section title="Advanced">
        <div className="settings-grid">
          <label className="settings-grid__label">Auto Sinks Update</label>
          <input
            type="checkbox"
            checked={!!form.auto_sinks_update}
            onChange={e => updateForm('auto_sinks_update', e.target.checked)}
          />

          <label className="settings-grid__label">Status File Path</label>
          <input
            type="text"
            value={form.status_file ?? ''}
            onChange={e => updateForm('status_file', e.target.value)}
          />
        </div>
        <div className="settings-section__actions">
          <button
            className="btn btn-primary"
            onClick={() => saveConfig(['auto_sinks_update', 'status_file'])}
          >
            Save Advanced
          </button>
        </div>
      </Section>
    </div>
  );
}
