//
//  Services.js
//
//  Copyright (c) 2019 2020 Andrea Bondavalli. All rights reserved.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//

import { toast } from 'react-toastify'; 

const API = '/api';
const config = '/config';
const streams = '/streams';
const sources = '/sources';
const sinks = '/sinks';
const ptpConfig = '/ptp/config';
const ptpStatus = '/ptp/status';
const source = '/source';
const sdp = '/sdp';
const sink = '/sink';
const status = '/status';

const defaultParams = {
  credentials: 'same-origin',
  redirect: 'error',
  headers: new Headers({
    'X-USER-ID': 'test'
  })
};

export default class RestAPI {

  static getBaseUrl() {
    return location.protocol + '//' + location.hostname + ':8080';
  }

  static doFetch(url, params = {}) {
    if (params.method === undefined) {
      params.method = 'GET';
    }

    return fetch(this.getBaseUrl() + API + url, Object.assign({}, defaultParams, params))
      .then(
        response => {
          if (response.ok) {
            return response;
          }
          console.log(this.getBaseUrl() + API + url + ' HTTP ' + response.status);
          return Promise.reject(Error('HTTP ' + response.status));
        }
      ).catch(
        err => {
          console.log(this.getBaseUrl() + API + url + ' failed: ' + err.message);
          return Promise.reject(Error(err.message));
        }
      );
  }

  static getConfig() {
    return this.doFetch(config).catch(err => {
      toast.error('Config get failed: ' + err.message);
      return Promise.reject(Error(err.message));
    });
  }

  static setConfig(log_severity, syslog_proto, syslog_server, rtp_mcast_base, rtp_port, playout_delay, tic_frame_size_at_1fs, sample_rate, max_tic_frame_size, sap_interval) {
    return this.doFetch(config, {
      body: JSON.stringify({
        log_severity: parseInt(log_severity, 10),
        syslog_proto: syslog_proto,
        syslog_server: syslog_server,
        rtp_mcast_base: rtp_mcast_base,
        rtp_port: parseInt(rtp_port, 10),
        playout_delay: parseInt(playout_delay, 10),
        tic_frame_size_at_1fs: parseInt(tic_frame_size_at_1fs, 10),
        sample_rate: parseInt(sample_rate, 10),
        max_tic_frame_size: parseInt(max_tic_frame_size, 10),
        sap_interval: parseInt(sap_interval, 10)
      }),
      method: 'POST'
    }).catch(err => {
      toast.error('Config set failed: ' + err.message)
      return Promise.reject(Error(err.message));
    });
  }

  static getPTPConfig() {
    return this.doFetch(ptpConfig).catch(err => {
      toast.error('PTP config get failed: ' + err.message);
      return Promise.reject(Error(err.message));
    });
  }

  static setPTPConfig(domain, dscp) {
    return this.doFetch(ptpConfig, {
      body: JSON.stringify({
        domain: parseInt(domain, 10),
        dscp: parseInt(dscp, 10)
      }),
      method: 'POST'
    }).catch(err => {
      toast.error('PTP config set failed: ' + err.message)
      return Promise.reject(Error(err.message));
    });
  }

  static addSource(id, enabled, name, io, max_samples_per_packet, codec, ttl, payload_type, dscp, refclk_ptp_traceable, map, is_edit) {
    return this.doFetch(source + '/' + id, {
      body: JSON.stringify({
        enabled: enabled,
        name: name,
        io: io,
        codec: codec,
        map: map,
        max_samples_per_packet: parseInt(max_samples_per_packet, 10),
        ttl: parseInt(ttl, 10),
        payload_type: parseInt(payload_type, 10),
        dscp: parseInt(dscp, 10),
        refclk_ptp_traceable: refclk_ptp_traceable
      }),
      method: 'PUT'
    }).catch(err => {
      toast.error((is_edit ? 'Update Source failed: ' : 'Add Source failed: ') + err.message)
      return Promise.reject(Error(err.message));
    });
  }

  static removeSource(id) {
    return this.doFetch(source + '/' + id, {
      method: 'DELETE'
    }).catch(err => {
      toast.error('Remove Source failed: ' + err.message)
      return Promise.reject(Error(err.message));
    });
  }

  static getSourceSDP(id) {
    return this.doFetch(source +  sdp + '/' + id, {
      method: 'GET'
    }).catch(err => {
      toast.error('Get Source SDP failed: ' + err.message)
      return Promise.reject(Error(err.message));
    });
  }

  static getSinkStatus(id) {
    return this.doFetch(sink +  status + '/' + id, {
      method: 'GET'
    }).catch(err => {
      toast.error('Get Sink status failed: ' + err.message)
      return Promise.reject(Error(err.message));
    });
  }

  static addSink(id, name, io, delay, use_sdp, source, sdp, ignore_refclk_gmid, map, is_edit) {
    return this.doFetch(sink + '/' + id, {
      body: JSON.stringify({
        name: name,
        io: io,
        delay: parseInt(delay, 10),
        use_sdp: use_sdp,
        source: source,
        sdp: sdp,
        ignore_refclk_gmid: ignore_refclk_gmid,
        map: map
      }),
      method: 'PUT'
    }).catch(err => {
      toast.error((is_edit ? 'Update Sink failed: ' : 'Add Sink failed: ') + err.message)
      return Promise.reject(Error(err.message));
    });
  }

  static removeSink(id) {
    return this.doFetch(sink + '/' + id, {
      method: 'DELETE'
    }).catch(err => {
      toast.error('Remove Sink failed: ' + err.message)
      return Promise.reject(Error(err.message));
    });
  }

  static getPTPStatus() {
    return this.doFetch(ptpStatus).catch(err => {
      toast.error('PTP status get failed: ' + err.message)
      return Promise.reject(Error(err.message));
    });
  }

  static getSources() {
    return this.doFetch(sources).catch(err => {
      toast.error('Sources get failed: ' + err.message)
      return Promise.reject(Error(err.message));
    });
  }

  static getSinks() {
    return this.doFetch(sinks).catch(err => {
      toast.error('Sinks get failed: ' + err.message)
      return Promise.reject(Error(err.message));
    });
  }

  static getStreams() {
    return this.doFetch(streams).catch(err => {
      toast.error('Streams get failed: ' + err.message)
      return Promise.reject(Error(err.message));
    });
  }

}
