import React, { useState, useEffect } from 'react';
import Modal from '../../components/shared/Modal';

const defaultSource = {
  id: 0,
  enabled: true,
  name: '',
  io: 'Audio Device',
  max_samples_per_packet: 48,
  codec: 'L24',
  address: '',
  ttl: 15,
  payload_type: 98,
  dscp: 34,
  refclk_ptp_traceable: true,
  map: [0],
};

export default function SourceEditModal({ isOpen, onClose, source, onSave }) {
  const isEdit = source != null;
  const [form, setForm] = useState(defaultSource);

  useEffect(() => {
    if (isOpen) {
      setForm(source ? { ...defaultSource, ...source } : { ...defaultSource });
    }
  }, [isOpen, source]);

  const setField = (field, value) => setForm((f) => ({ ...f, [field]: value }));

  const channels = form.map ? form.map.length : 1;

  const setChannels = (count) => {
    const n = Math.max(1, Math.min(64, count));
    const start = form.map && form.map.length > 0 ? form.map[0] : 0;
    const newMap = [];
    for (let i = 0; i < n; i++) {
      newMap.push(start + i);
    }
    setField('map', newMap);
  };

  const setMapEntry = (index, value) => {
    const newMap = [...form.map];
    newMap[index] = parseInt(value, 10) || 0;
    setField('map', newMap);
  };

  const handleSubmit = (e) => {
    e.preventDefault();
    const payload = {
      enabled: form.enabled,
      name: form.name,
      io: form.io,
      max_samples_per_packet: parseInt(form.max_samples_per_packet, 10),
      codec: form.codec,
      address: form.address || '',
      ttl: parseInt(form.ttl, 10),
      payload_type: parseInt(form.payload_type, 10),
      dscp: parseInt(form.dscp, 10),
      refclk_ptp_traceable: form.refclk_ptp_traceable,
      map: form.map,
    };
    onSave(parseInt(form.id, 10), payload);
  };

  return (
    <Modal
      isOpen={isOpen}
      onClose={onClose}
      title={isEdit ? `Edit Source ${form.id}` : 'Add Source'}
      width="520px"
    >
      <form className="source-form" onSubmit={handleSubmit}>
        <div className="source-form__row">
          <label className="source-form__label">ID</label>
          <input
            className="source-form__input"
            type="number"
            min="0"
            max="63"
            value={form.id}
            onChange={(e) => setField('id', e.target.value)}
            disabled={isEdit}
            required
          />
        </div>

        <div className="source-form__checkbox-row">
          <input
            type="checkbox"
            id="src-enabled"
            checked={form.enabled}
            onChange={(e) => setField('enabled', e.target.checked)}
          />
          <label htmlFor="src-enabled">Enabled</label>
        </div>

        <div className="source-form__row">
          <label className="source-form__label">Name</label>
          <input
            className="source-form__input"
            type="text"
            value={form.name}
            onChange={(e) => setField('name', e.target.value)}
            required
          />
        </div>

        <div className="source-form__row">
          <label className="source-form__label">IO</label>
          <input
            className="source-form__input"
            type="text"
            value={form.io}
            onChange={(e) => setField('io', e.target.value)}
          />
        </div>

        <div className="source-form__grid">
          <div className="source-form__row">
            <label className="source-form__label">Max Samples/Packet</label>
            <select
              className="source-form__input"
              value={form.max_samples_per_packet}
              onChange={(e) => setField('max_samples_per_packet', e.target.value)}
            >
              <option value="6">6</option>
              <option value="12">12</option>
              <option value="16">16</option>
              <option value="48">48</option>
              <option value="96">96</option>
              <option value="192">192</option>
            </select>
          </div>

          <div className="source-form__row">
            <label className="source-form__label">Codec</label>
            <select
              className="source-form__input"
              value={form.codec}
              onChange={(e) => setField('codec', e.target.value)}
            >
              <option value="L16">L16</option>
              <option value="L24">L24</option>
              <option value="AM824">AM824</option>
              <option value="L32">L32</option>
            </select>
          </div>
        </div>

        <div className="source-form__row">
          <label className="source-form__label">Multicast Address</label>
          <input
            className="source-form__input"
            type="text"
            value={form.address}
            onChange={(e) => setField('address', e.target.value)}
            placeholder="e.g. 239.69.0.1"
          />
        </div>

        <div className="source-form__grid">
          <div className="source-form__row">
            <label className="source-form__label">TTL</label>
            <input
              className="source-form__input"
              type="number"
              min="1"
              max="255"
              value={form.ttl}
              onChange={(e) => setField('ttl', e.target.value)}
              required
            />
          </div>

          <div className="source-form__row">
            <label className="source-form__label">Payload Type</label>
            <input
              className="source-form__input"
              type="number"
              min="77"
              max="127"
              value={form.payload_type}
              onChange={(e) => setField('payload_type', e.target.value)}
              required
            />
          </div>
        </div>

        <div className="source-form__row">
          <label className="source-form__label">DSCP</label>
          <select
            className="source-form__input"
            value={form.dscp}
            onChange={(e) => setField('dscp', e.target.value)}
          >
            <option value="56">56 (CS7)</option>
            <option value="48">48 (CS6)</option>
            <option value="46">46 (EF)</option>
            <option value="36">36 (AF42)</option>
            <option value="34">34 (AF41)</option>
            <option value="0">0 (BE)</option>
          </select>
        </div>

        <div className="source-form__checkbox-row">
          <input
            type="checkbox"
            id="src-refclk"
            checked={form.refclk_ptp_traceable}
            onChange={(e) => setField('refclk_ptp_traceable', e.target.checked)}
          />
          <label htmlFor="src-refclk">RefClk PTP Traceable</label>
        </div>

        <div className="source-form__row">
          <label className="source-form__label">Channels</label>
          <input
            className="source-form__input"
            type="number"
            min="1"
            max="64"
            value={channels}
            onChange={(e) => setChannels(parseInt(e.target.value, 10) || 1)}
            required
          />
        </div>

        <div className="source-form__row">
          <label className="source-form__label">Channel Map</label>
          <div className="source-form__map-list">
            {(form.map || []).map((val, i) => (
              <input
                key={i}
                className="source-form__map-input"
                type="number"
                min="0"
                max="63"
                value={val}
                onChange={(e) => setMapEntry(i, e.target.value)}
              />
            ))}
          </div>
        </div>

        <div className="source-form__footer">
          <button type="button" className="btn" onClick={onClose}>Cancel</button>
          <button type="submit" className="btn btn-primary">
            {isEdit ? 'Save Changes' : 'Add Source'}
          </button>
        </div>
      </form>
    </Modal>
  );
}
