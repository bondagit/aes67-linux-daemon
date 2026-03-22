const BASE = '/api';
const headers = { 'Content-Type': 'application/json' };

async function request(url, options = {}) {
  const res = await fetch(`${BASE}${url}`, { ...options, headers });
  if (!res.ok) throw new Error(`API error: ${res.status}`);
  return res;
}

export const api = {
  getVersion: () => request('/version').then(r => r.json()),
  getConfig: () => request('/config').then(r => r.json()),
  setConfig: (data) => request('/config', { method: 'POST', body: JSON.stringify(data) }),
  getPTPStatus: () => request('/ptp/status').then(r => r.json()),
  getPTPConfig: () => request('/ptp/config').then(r => r.json()),
  setPTPConfig: (data) => request('/ptp/config', { method: 'POST', body: JSON.stringify(data) }),
  getSources: () => request('/sources').then(r => r.json()),
  getSourceSDP: (id) => request(`/source/sdp/${id}`).then(r => r.text()),
  addSource: (id, data) => request(`/source/${id}`, { method: 'PUT', body: JSON.stringify(data) }),
  removeSource: (id) => request(`/source/${id}`, { method: 'DELETE' }),
  getSinks: () => request('/sinks').then(r => r.json()),
  getSinkStatus: (id) => request(`/sink/status/${id}`).then(r => r.json()),
  addSink: (id, data) => request(`/sink/${id}`, { method: 'PUT', body: JSON.stringify(data) }),
  removeSink: (id) => request(`/sink/${id}`, { method: 'DELETE' }),
  getBrowseSources: () => request('/browse/sources/all').then(r => r.json()),
  getMatrix: () => request('/matrix').then(r => r.json()),
  setMatrix: (data) => request('/matrix', { method: 'PUT', body: JSON.stringify(data) }),
  setMatrixRoute: (data) => request('/matrix/route', { method: 'PUT', body: JSON.stringify(data) }),
};
