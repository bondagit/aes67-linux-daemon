//
//  Config.js
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

import React, {Component} from 'react';
import { toast } from 'react-toastify'; 

import RestAPI from './Services';
import Loader from './Loader';

require('./styles.css');

class Config extends Component {
  constructor(props) {
    super(props);
    this.state = {
      httpPort: '',
      logSeverity: '',
      playoutDelay: '',
      playoutDelayErr: false,
      ticFrameSizeAt1fs: '',
      ticFrameSizeAt1fsErr: false,
      maxTicFrameSize: '',
      maxTicFrameSizeErr: false,
      sampleRate: '',
      rtpMcastBase: '',
      rtpMcastBaseErr: false,
      rtpPort: '',
      rtpPortErr: false,
      ptpDomain: '',
      ptpDscp: '',
      sapInterval: '',
      sapIntervalErr: false,
      syslogProto: '',
      syslogServer: '',
      syslogServerErr: false,
      statusFile: '',
      interfaceName: '',
      macAddr: '',
      ipAddr: '',
      errors: 0,
      isConfigLoading: false
    };
    this.onSubmit = this.onSubmit.bind(this);
    this.inputIsValid = this.inputIsValid.bind(this);
  }

  componentDidMount() {
    this.setState({isConfigLoading: true});
    RestAPI.getConfig()
      .then(response => response.json())
      .then(
        data => this.setState(
          {
            httpPort: data.http_port,
            logSeverity: data.log_severity,
            playoutDelay: data.playout_delay,
            ticFrameSizeAt1fs: data.tic_frame_size_at_1fs,
            maxTicFrameSize: data.max_tic_frame_size,
            sampleRate: data.sample_rate,
            rtpMcastBase: data.rtp_mcast_base,
            rtpPort: data.rtp_port,
            ptpDomain: data.ptp_domain,
            ptpDscp: data.ptp_dscp,
            sapMcastAddr: data.sap_mcast_addr,
            sapInterval: data.sap_interval,
            syslogProto: data.syslog_proto,
            syslogServer: data.syslog_server,
            statusFile: data.status_file,
            interfaceName: data.interface_name,
            macAddr: data.mac_addr,
            ipAddr: data.ip_addr,
            isConfigLoading: false
	  }))
      .catch(err => this.setState({isConfigLoading: false}));
  }
  
  inputIsValid() {
    return !this.state.playoutDelayErr &&
      !this.state.ticFrameSizeAt1fsErr &&
      !this.state.maxTicFrameSizeErr &&
      !this.state.rtpMcastBaseErr &&
      !this.state.sapMcastAddrErr &&
      !this.state.rtpPortErr &&
      !this.state.sapIntervalErr &&
      !this.state.syslogServerErr &&
      !this.state.isConfigLoading;
  }

  onSubmit(event) {
    event.preventDefault();
    RestAPI.setConfig(
      this.state.logSeverity, 
      this.state.syslogProto, 
      this.state.syslogServer, 
      this.state.rtpMcastBase, 
      this.state.rtpPort, 
      this.state.playoutDelay, 
      this.state.ticFrameSizeAt1fs, 
      this.state.sampleRate, 
      this.state.maxTicFrameSize, 
      this.state.sapMcastAddr,
      this.state.sapInterval)
    .then(response => toast.success('Config updated, daemon restart ...'));
  }
  
  render() {
    return (
      <div className='config'>
	{this.state.isConfigLoading ? <Loader/> : <h3>Audio Config</h3>}
        <table><tbody>
          <tr>
            <th align="left"> <label>Safety Playout delay (samples) </label> </th>
            <th align="left"> <input type='number' min='0' max='4000' className='input-number' value={this.state.playoutDelay} onChange={e => this.setState({playoutDelay: e.target.value, playoutDelayErr: !e.currentTarget.checkValidity()})} required/> </th>
          </tr>
          <tr>
            <th align="left"> <label>TIC Frame size at @1FS </label> </th>
            <th align="left"> <input type='number' min='192' max='8192' className='input-number' value={this.state.ticFrameSizeAt1fs} onChange={e => this.setState({ticFrameSizeAt1fs: e.target.value, ticFrameSizeAt1fsErr: !e.currentTarget.checkValidity()})} disabled required/> </th>
          </tr>
          <tr>
            <th align="left"> <label>Max TIC frame size </label> </th>
            <th align="left"> <input type='number' min='192' max='8192' className='input-number' value={this.state.maxTicFrameSize} onChange={e => this.setState({maxTicFrameSize: e.target.value, maxTicFrameSizeErr: !e.currentTarget.checkValidity()})} disabled required/> </th>
          </tr>
          <tr>
            <th align="left"> <label>Initial Sample rate</label> </th>
            <th align="left"> 
	      <select value={this.state.sampleRate} onChange={e => this.setState({sampleRate: e.target.value})}>
                <option value="44100">44.1 kHz</option>
                <option value="48000">48 kHz</option>
                <option value="96000">96 kHz</option>
              </select>
            </th>
          </tr>
        </tbody></table>
        <br/>
	{this.state.isConfigLoading ? <Loader/> : <h3>Network Config</h3>}
        <table><tbody>
          <tr>
            <th align="left"> <label>HTTP port</label> </th>
            <th align="left"> <input type='number' min='1024' max='65536' className='input-number' value={this.state.httpPort} disabled/> </th>
          </tr>
          <tr>
            <th align="left"> <label>RTP base address</label> </th>
            <th align="left"> <input type="text" minLength="7" maxLength="15" size="15" pattern="^2(?:2[4-9]|3\d)(?:\.(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]\d?|0)){3}$" value={this.state.rtpMcastBase} onChange={e => this.setState({rtpMcastBase: e.target.value, rtpMcastBaseErr: !e.currentTarget.checkValidity()})} required/> </th>
          </tr>
          <tr>
            <th align="left"> <label>RTP port</label> </th>
            <th align="left"> <input type='number' min='1024' max='65536'  className='input-number' value={this.state.rtpPort} onChange={e => this.setState({rtpPort: e.target.value, rtpPortErr: !e.currentTarget.checkValidity()})} required/> </th>
          </tr>
          <tr>
            <th align="left"> <label>SAP multicast address</label> </th>
            <th align="left"> <input type="text" minLength="7" maxLength="15" size="15" pattern="^2(?:2[4-9]|3\d)(?:\.(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]\d?|0)){3}$" value={this.state.sapMcastAddr} onChange={e => this.setState({sapMcastAddr: e.target.value, sapMcastAddrErr: !e.currentTarget.checkValidity()})} required/> </th>
          </tr>
          <tr>
            <th align="left"> <label>SAP interval (sec)</label> </th>
            <th align="left"> <input type='number' min='0' max='255'  className='input-number' value={this.state.sapInterval} onChange={e => this.setState({sapInterval: e.target.value, sapIntervalErr: !e.currentTarget.checkValidity()})} required/> </th>
          </tr>
          <tr>
            <th align="left"> <label>Network Interface</label> </th>
            <th align="left"> <input value={this.state.interfaceName} disabled/> </th>
          </tr>
          <tr>
            <th align="left"> <label>MAC address</label> </th>
            <th align="left"> <input value={this.state.macAddr} disabled/> </th>
          </tr>
          <tr>
            <th align="left"> <label>IP address</label> </th>
            <th align="left"> <input value={this.state.ipAddr} disabled/> </th>
          </tr>
        </tbody></table>
        <br/>
	{this.state.isConfigLoading ? <Loader/> : <h3>Logging Config</h3>}
        <table><tbody>
          <tr>
            <th align="left"> <label>Syslog protocol</label> </th>
            <th align="left"> 
	      <select value={this.state.syslogProto} onChange={e => this.setState({syslogProto: e.target.value})}>
                <option value="none">none</option>
                <option value="">local</option>
                <option value="udp">UDP server</option>
              </select>
            </th>
          </tr>
          <tr>
            <th align="left"> <label>Syslog server</label> </th>
            <th align="left"> <input value={this.state.syslogServer} onChange={e => this.setState({syslogServer: e.target.value, syslogServerErr: !e.currentTarget.checkValidity()})} disabled={this.state.syslogProto === 'udp' ? undefined : true} /> </th>
          </tr>
          <tr>
            <th align="left"> <label>Log severity</label> </th>
            <th align="left"> 
	      <select value={this.state.logSeverity} onChange={e => this.setState({logSeverity: e.target.value})}>
                <option value="1">debug</option>
                <option value="2">info</option>
                <option value="3">warning</option>
                <option value="4">error</option>
                <option value="5">fatal</option>
              </select>
            </th>
          </tr>
        </tbody></table>
        <br/>
        <table><tbody>
          <tr>
            <th> <button disabled={this.inputIsValid() ? undefined : true} onClick={this.onSubmit}>Submit</button> </th>
          </tr>
        </tbody></table>
      </div>
    )
  }
}

export default Config;
