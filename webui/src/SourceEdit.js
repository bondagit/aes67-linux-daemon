//
//  SourceEdit.js
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
import PropTypes from 'prop-types';
import {toast} from 'react-toastify';
import Modal from 'react-modal';

import RestAPI from './Services';

require('./styles.css');

const editCustomStyles = {
  content : {
    top:  '50%',
    left: '50%',
    right: 'auto',
    bottom: 'auto',
    marginRight: '-50%',
    transform: 'translate(-50%, -50%)'
  }
};

const max_packet_size = 1440; //bytes 

class SourceEdit extends Component {
  static propTypes = {
    source: PropTypes.object.isRequired,
    applyEdit: PropTypes.func.isRequired,
    closeEdit: PropTypes.func.isRequired,
    editIsOpen: PropTypes.bool.isRequired,
    isEdit: PropTypes.bool.isRequired,
    editTitle: PropTypes.string.isRequired
  };

  constructor(props) {
    super(props);
    this.state = {
      id: this.props.source.id,
      enabled: this.props.source.enabled,
      name: this.props.source.name,
      nameErr: false,
      io: this.props.source.io,
      maxSamplesPerPacket: this.props.source.max_samples_per_packet,
      codec: this.props.source.codec,
      ttl: this.props.source.ttl,
      ttlErr: false,
      payloadType: this.props.source.payload_type,
      payloadTypeErr: false,
      dscp: this.props.source.dscp,
      refclkPtpTraceable: this.props.source.refclk_ptp_traceable,
      channels: this.props.source.map.length,
      channelsErr: false,
      maxChannels: Math.floor(max_packet_size / (this.props.source.max_samples_per_packet * (this.props.source.codec === 'L16' ? 2 : 3))),
      map: this.props.source.map,
      audioMap: []
    }
    let v;
    for (v = 0; v <= (64 - this.state.channels); v += 1) {
      this.state.audioMap.push(v);
    }
    this.onSubmit = this.onSubmit.bind(this);
    this.onCancel = this.onCancel.bind(this);
    this.addSource = this.addSource.bind(this);
    this.onChangeChannels = this.onChangeChannels.bind(this);
    this.onChangeChannelsMap = this.onChangeChannelsMap.bind(this);
    this.onChangeMaxSamplesPerPacket = this.onChangeMaxSamplesPerPacket.bind(this);
    this.onChangeCodec = this.onChangeCodec.bind(this);
    this.inputIsValid = this.inputIsValid.bind(this);
  }

  componentDidMount() {
    Modal.setAppElement('body');
  }

  addSource(message) {
    RestAPI.addSource(
      this.state.id,
      this.state.enabled,
      this.state.name,
      this.state.io,
      this.state.maxSamplesPerPacket,
      this.state.codec,
      this.state.ttl,
      this.state.payloadType,
      this.state.dscp,
      this.state.refclkPtpTraceable,
      this.state.map,
      this.props.isEdit)
    .then(function(response) { 
      toast.success(message);
      this.props.applyEdit();
    }.bind(this));
  }

  onSubmit() {
    this.addSource('Source ' + this.state.id + (this.props.isEdit ? ' updated ' : ' added'));
  }

  onCancel() {
    this.props.closeEdit();
  }

  onChangeMaxSamplesPerPacket(e) {
    let samples = parseInt(e.target.value, 10);
    let maxChannels = Math.floor(max_packet_size / (samples * (this.state.codec === 'L16' ? 2 : 3)));
    this.setState({ maxSamplesPerPacket: samples, maxChannels: maxChannels, channelsErr: this.state.channels > maxChannels });
  }

  onChangeCodec(e) {
    let codec = e.target.value;
    let maxChannels = Math.floor(max_packet_size / (this.state.maxSamplesPerPacket * (codec === 'L16' ? 2 : 3)));
    this.setState({ codec: codec, maxChannels: maxChannels, channelsErr: this.state.channels > maxChannels });
  }
 
  onChangeChannels(e) {
    if (e.currentTarget.checkValidity()) {
      let channels = parseInt(e.target.value, 10);
      let audioMap = [], map = [], v;
      for (v = 0; v <= (64 - channels); v += 1) {
        audioMap.push(v);
      }
      for (v = 0; v < channels; v++) {
        map.push(v + this.state.map[0]);
      }
      this.setState({ map: map, channels: channels, audioMap: audioMap, channelsErr: false });
    }
  }

  onChangeChannelsMap(e) {
    let startChannel = parseInt(e.target.value, 10);
    let map = [], v;
    for (v = 0; v < this.state.channels; v++) {
      map.push(v + startChannel);
    }
    this.setState({ map: map });
  }

  inputIsValid() {
    return !this.state.nameErr &&
      !this.state.ttlErr &&
      !this.state.channelsErr &&
      !this.state.payloadTypeErr;
  }

  render()  {
    return (
      <div id='source-edit'>
        <Modal ariaHideApp={false} 
          isOpen={this.props.editIsOpen}
          onRequestClose={this.props.closeEdit}
          style={editCustomStyles}
          contentLabel="Source Edit">
          <h2><center>{this.props.editTitle}</center></h2>
          <table><tbody>
            <tr>
              <th align="left"> <font color='grey'>ID</font> </th>
              <th align="left"> <input type='number' min='0' max='63' className='input-number' value={this.state.id} onChange={e => this.setState({id: e.target.value})} disabled required/> </th>
            </tr>
            <tr height="35">
              <th align="left"> <label>Enabled</label> </th>
              <th align="left"> <input type="checkbox" defaultChecked={this.state.enabled} onChange={e => this.setState({enabled: e.target.checked})} /> </th>
            </tr>
            <tr>
              <th align="left"> <label>Name</label> </th>
              <th align="left"> <input value={this.state.name} onChange={e => this.setState({name: e.target.value, nameErr: !e.currentTarget.checkValidity()})} required/> </th>
            </tr>
            <tr>
              <th align="left"> <label>Max samples per packet </label> </th>
              <th align="left"> 
	        <select value={this.state.maxSamplesPerPacket} onChange={this.onChangeMaxSamplesPerPacket}>
                  <option value="6">6 - 125&mu;s@48Khz</option>
                  <option value="12">12 - 250&mu;s@48Khz</option>
                  <option value="16">16 - 333&mu;s@48Khz</option>
                  <option value="48">48 - 1ms@48Khz</option>
                  <option value="96">96 - 2ms@48Khz</option>
                  <option value="192">192 - 4ms@48Khz</option>
                </select>
              </th>
            </tr>
            <tr>
              <th align="left"> <label>Codec</label> </th>
              <th align="left"> 
	        <select value={this.state.codec} onChange={this.onChangeCodec}>
                  <option value="L16">L16</option>
                  <option value="L24">L24</option>
                </select>
              </th>
            </tr>
            <tr>
              <th align="left"> <label>Payload Type</label> </th>
              <th align="left"> <input type='number' min='77' max='127' className='input-number' value={this.state.payloadType} onChange={e => this.setState({payloadType: e.target.value, payloadTypeErr: !e.currentTarget.checkValidity()})} required/> </th>
            </tr>
            <tr>
              <th align="left"> <label>TTL</label> </th>
              <th align="left"> <input type='number' min='1' max='255' className='input-number' value={this.state.ttl} onChange={e => this.setState({ttl: e.target.value, ttlErr: !e.currentTarget.checkValidity()})} required/> </th>
            </tr>
            <tr>
              <th align="left"> <label>DSCP</label> </th>
              <th align="left">
                <select value={this.state.dscp} onChange={e => this.setState({dscp: e.target.value})}>
                  <option value="46">46 (EF)</option>
                  <option value="34">34 (AF41)</option>
                  <option value="36">26 (AF31)</option>
                  <option value="48">0 (BE)</option>
                </select>
              </th>
            </tr>
            <tr height="35">
              <th align="left"> <label>RefClk PTP traceable</label> </th>
              <th align="left"> <input type="checkbox" defaultChecked={this.state.refclkPtpTraceable} onChange={e => this.setState({refclkPtpTraceable: e.target.checked})} /> </th>
            </tr>
            <tr>
              <th align="left"> <label>Channels</label> </th>
              <th align="left"> <input type='number' min='1' max={this.state.maxChannels} className='input-number' value={this.state.channels} onChange={this.onChangeChannels} required/> </th>
            </tr>
            <tr>
              <th align="left">Audio Channels map</th>
              <th align="left">
                <select value={this.state.map[0]} onChange={this.onChangeChannelsMap}>
                  { this.state.channels > 1 ? 
                      this.state.audioMap.map((v) => <option key={v} value={v}>{'ALSA Output ' + parseInt(v + 1, 10) + ' -> ALSA Output ' +  parseInt(v + this.state.channels, 10)}</option>) : 
                      this.state.audioMap.map((v) => <option key={v} value={v}>{'ALSA Output ' + parseInt(v + 1, 10)}</option>)
		  }
                </select>
              </th>
            </tr>
          </tbody></table>
          <br/>
	  <div style={{textAlign: 'center'}}>
            <button onClick={this.onSubmit} disabled={this.inputIsValid() ? undefined : true}>Submit</button>
            &nbsp;&nbsp;&nbsp;&nbsp;
            <button onClick={this.onCancel}>Cancel</button>
          </div>
        </Modal>
      </div>
    );
  }
}

export default SourceEdit;
