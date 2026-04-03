import React, { useState, useEffect } from 'react';
import Modal from '../../components/shared/Modal';
import { api } from '../../services/api';

const defaultSink = {
  id: 0,
  name: '',
  io: 'Audio Device',
  source: '',
  use_sdp: false,
  sdp: '',
  delay: 576,
  ignore_refclk_gmid: false,
  map: [0],
};

export default function SinkEditModal({ isOpen, onClose, sink, onSave }) {
  const isEdit = sink !== null && !sink?._prefill;
  const initial = sink ? { ...defaultSink, ...sink } : defaultSink;

  const [id, setId] = useState(initial.id);
  const [name, setName] = useState(initial.name);
  const [io, setIo] = useState(initial.io);
  const [source, setSource] = useState(initial.source);
  const [useSdp, setUseSdp] = useState(initial.use_sdp);
  const [sdp, setSdp] = useState(initial.sdp);
  const [delay, setDelay] = useState(initial.delay);
  const [ignoreRefclkGmid, setIgnoreRefclkGmid] = useState(initial.ignore_refclk_gmid);
  const [channels, setChannels] = useState(initial.map ? initial.map.length : 1);
  const [map, setMap] = useState(initial.map || [0]);
  const [remoteSources, setRemoteSources] = useState([]);

  useEffect(() => {
    const s = sink || defaultSink;
    setId(s.id);
    setName(s.name);
    setIo(s.io || 'Audio Device');
    setSource(s.source || '');
    setUseSdp(s.use_sdp || false);
    setSdp(s.sdp || '');
    setDelay(s.delay || 576);
    setIgnoreRefclkGmid(s.ignore_refclk_gmid || false);
    setChannels(s.map ? s.map.length : 1);
    setMap(s.map || [0]);
  }, [sink, isOpen]);

  useEffect(() => {
    if (isOpen) {
      api.getBrowseSources()
        .then(data => {
          if (data && data.remote_sources) {
            setRemoteSources(data.remote_sources);
          }
        })
        .catch(() => {});
    }
  }, [isOpen]);

  const audioMapOptions = [];
  for (let v = 0; v <= 64 - channels; v++) {
    audioMapOptions.push(v);
  }

  const handleChannelsChange = (e) => {
    const val = parseInt(e.target.value, 10);
    if (isNaN(val) || val < 1 || val > 64) return;
    const newMap = [];
    const start = map[0] || 0;
    for (let i = 0; i < val; i++) {
      newMap.push(i + start);
    }
    setChannels(val);
    setMap(newMap);
  };

  const handleMapChange = (e) => {
    const start = parseInt(e.target.value, 10);
    const newMap = [];
    for (let i = 0; i < channels; i++) {
      newMap.push(i + start);
    }
    setMap(newMap);
  };

  const handleRemoteSourceSDP = (e) => {
    if (e.target.value) {
      setSdp(e.target.value);
    }
  };

  const isValid = () => {
    if (!name) return false;
    if (!useSdp && !source) return false;
    if (useSdp && !sdp) return false;
    return true;
  };

  const handleSubmit = async () => {
    const sinkData = {
      name,
      io,
      source: source || '',
      use_sdp: useSdp,
      sdp: sdp || '',
      delay: parseInt(delay, 10),
      ignore_refclk_gmid: ignoreRefclkGmid,
      map,
    };
    try {
      await api.addSink(id, sinkData);
      onSave();
    } catch (err) {
      console.error('Failed to save sink:', err);
    }
  };

  return (
    <Modal isOpen={isOpen} onClose={onClose} title={isEdit ? 'Edit Sink' : 'Add Sink'} width="580px">
      <div className="sink-form">
        <div className="sink-form-row">
          <div className="sink-form-group">
            <label>ID</label>
            <input type="number" min={0} max={63} value={id} onChange={e => setId(parseInt(e.target.value, 10))} disabled={isEdit} />
          </div>
          <div className="sink-form-group">
            <label>Name</label>
            <input type="text" value={name} onChange={e => setName(e.target.value)} required />
          </div>
        </div>

        <div className="sink-form-group">
          <label>IO</label>
          <input type="text" value={io} onChange={e => setIo(e.target.value)} />
        </div>

        <div className="sink-form-check">
          <input type="checkbox" id="sink-use-sdp" checked={useSdp} onChange={e => setUseSdp(e.target.checked)} />
          <label htmlFor="sink-use-sdp">Use SDP</label>
        </div>

        <div className="sink-form-group">
          <label style={{ opacity: useSdp ? 0.4 : 1 }}>Source URL</label>
          <input type="url" value={source} onChange={e => setSource(e.target.value)} disabled={useSdp} placeholder="http://" />
        </div>

        <div className="sink-form-group">
          <label style={{ opacity: !useSdp ? 0.4 : 1 }}>Remote Source SDP</label>
          <select value={sdp} onChange={handleRemoteSourceSDP} disabled={!useSdp}>
            <option value="">-- select a remote source SDP --</option>
            {remoteSources.map((rs) => (
              <option key={rs.id} value={rs.sdp}>
                {rs.source + ' from ' + rs.address + ' - ' + rs.name}
              </option>
            ))}
          </select>
        </div>

        <div className="sink-form-group">
          <label style={{ opacity: !useSdp ? 0.4 : 1 }}>SDP</label>
          <textarea rows={10} value={sdp} onChange={e => setSdp(e.target.value)} disabled={!useSdp} />
        </div>

        <div className="sink-form-group">
          <label>Delay (samples)</label>
          <select value={delay} onChange={e => setDelay(e.target.value)}>
            <option value={192}>192 - 4ms@48kHz</option>
            <option value={384}>384 - 8ms@48kHz</option>
            <option value={576}>576 - 12ms@48kHz</option>
            <option value={768}>768 - 16ms@48kHz</option>
            <option value={960}>960 - 20ms@48kHz</option>
          </select>
        </div>

        <div className="sink-form-check">
          <input type="checkbox" id="sink-ignore-gmid" checked={ignoreRefclkGmid} onChange={e => setIgnoreRefclkGmid(e.target.checked)} />
          <label htmlFor="sink-ignore-gmid">Ignore RefClk GMID</label>
        </div>

        <div className="sink-form-row">
          <div className="sink-form-group">
            <label>Channels</label>
            <input type="number" min={1} max={64} value={channels} onChange={handleChannelsChange} />
          </div>
          <div className="sink-form-group">
            <label>Audio Channels Map</label>
            <select value={map[0]} onChange={handleMapChange}>
              {audioMapOptions.map((v) => (
                <option key={v} value={v}>
                  {channels > 1
                    ? `ALSA Input ${v + 1} -> ALSA Input ${v + channels}`
                    : `ALSA Input ${v + 1}`}
                </option>
              ))}
            </select>
          </div>
        </div>

        <div className="sink-form-actions">
          <button className="sink-btn-cancel" onClick={onClose}>Cancel</button>
          <button className="sink-btn-save" onClick={handleSubmit} disabled={!isValid()}>
            {isEdit ? 'Update' : 'Add'} Sink
          </button>
        </div>
      </div>
    </Modal>
  );
}
