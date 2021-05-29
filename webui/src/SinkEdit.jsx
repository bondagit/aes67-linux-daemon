//
//  SinkEdit.jsx
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

class SinkEdit extends Component {
  static propTypes = {
    sink: PropTypes.object.isRequired,
    applyEdit: PropTypes.func.isRequired,
    closeEdit: PropTypes.func.isRequired,
    editIsOpen: PropTypes.bool.isRequired,
    isEdit: PropTypes.bool.isRequired,
    editTitle: PropTypes.string.isRequired
  };

  constructor(props) {
    super(props);
    this.state = {
      sources: [],
      id: this.props.sink.id,
      name: this.props.sink.name,
      nameErr: false,
      io: this.props.sink.io,
      delay: this.props.sink.delay,
      ignoreRefclkGmid: this.props.sink.ignore_refclk_gmid,
      useSdp: this.props.sink.use_sdp,
      source: this.props.sink.source,
      sourceErr: false,
      sdp: this.props.sink.sdp,
      channels: this.props.sink.map.length,
      map: this.props.sink.map,
      audioMap: []
    }
    let v;
    for (v = 0; v <= (64 - this.state.channels); v += 1) {
      this.state.audioMap.push(v);
    }
    this.onSubmit = this.onSubmit.bind(this);
    this.onCancel = this.onCancel.bind(this);
    this.addSink = this.addSink.bind(this);
    this.onChangeChannels = this.onChangeChannels.bind(this);
    this.onChangeChannelsMap = this.onChangeChannelsMap.bind(this);
    this.inputIsValid = this.inputIsValid.bind(this);
    this.fetchRemoteSources = this.fetchRemoteSources.bind(this);
    this.onChangeRemoteSourceSDP = this.onChangeRemoteSourceSDP.bind(this);
  }

  fetchRemoteSources() {
    RestAPI.getRemoteSources()
      .then(response => response.json())
      .then(
        data => this.setState( { sources: data.remote_sources }))
  }

  componentDidMount() {
    Modal.setAppElement('body');
    this.fetchRemoteSources();
  }

  addSink(message) {
    RestAPI.addSink(
      this.state.id,
      this.state.name,
      this.state.io,
      this.state.delay,
      this.state.useSdp,
      this.state.source ? this.state.source : '',
      this.state.sdp ? this.state.sdp : '',
      this.state.ignoreRefclkGmid,
      this.state.map,
      this.props.isEdit)
    .then(function(response) {
      this.props.applyEdit();
      toast.success(message);
    }.bind(this));
  }

  onSubmit() {
    this.addSink('Sink ' + this.state.id + (this.props.isEdit ? ' updated ' : ' added'));
  }

  onCancel() {
    this.props.closeEdit();
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
      this.setState({ map: map, channels: channels, audioMap: audioMap });
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

  onChangeRemoteSourceSDP(e) {
    if (e.target.value) {
      this.setState({ sdp: e.target.value });
    }
  }

  inputIsValid() {
    return !this.state.nameErr &&
      !this.state.sourceErr &&
      (this.state.useSdp || this.state.source) &&
      (!this.state.useSdp || this.state.sdp);
  }

  render()  {
    return (
      <div id='sink-edit'>
        <Modal ariaHideApp={false}
          isOpen={this.props.editIsOpen}
          onRequestClose={this.props.closeEdit}
          style={editCustomStyles}
          contentLabel="Sink Edit">
          <h2><center>{this.props.editTitle}</center></h2>
          <table><tbody>
            <tr>
              <th align="left"> <font color='grey'>ID</font> </th>
              <th align="left"> <input type='number' min='0' max='63' className='input-number' value={this.state.id} onChange={e => this.setState({id: e.target.value})} disabled required/> </th>
            </tr>
            <tr>
              <th align="left"> <label>Name</label> </th>
              <th align="left"> <input value={this.state.name} onChange={e => this.setState({name: e.target.value, nameErr: !e.currentTarget.checkValidity()})} required/> </th>
            </tr>
            <tr height="35">
              <th align="left"> <label>Use SDP</label> </th>
              <th align="left"> <input type="checkbox" defaultChecked={this.state.useSdp} onChange={e => this.setState({useSdp: e.target.checked})} /> </th>
            </tr>
            <tr>
              <th align="left"> <font color={this.state.useSdp ? 'grey' : 'black'}>Source URL</font> </th>
              <th align="left"> <input type='url' size="30" value={this.state.source} onChange={e => this.setState({source: e.target.value, sourceErr: !e.currentTarget.checkValidity()})} disabled={this.state.useSdp ? true : undefined} required/> </th>
            </tr>
            <tr>
              <th align="left"> <font color={!this.state.useSdp ? 'grey' : 'black'}>Remote Source SDP</font> </th>
              <th align="left">
                <select value={this.state.sdp} onChange={this.onChangeRemoteSourceSDP} disabled={this.state.useSdp ? undefined : true}>
                  <option key='' value=''> -- select a remote source SDP -- </option>
                  {
                    this.state.sources.map((v) => <option key={v.id} value={v.sdp}>{v.source + ' from ' + v.address + ' - ' + v.name}</option>)
		  }
                </select>
              </th>
            </tr>
            <tr>
              <th align="left"> <font color={!this.state.useSdp ? 'grey' : 'black'}>SDP</font> </th>
              <th align="left"> <textarea rows='15' cols='55' value={this.state.sdp} onChange={e => this.setState({sdp: e.target.value})} disabled={this.state.useSdp ? undefined : true} required/> </th>
            </tr>
            <tr>
              <th align="left"> <label>Delay (samples) </label> </th>
              <th align="left">
	        <select value={this.state.delay} onChange={e => this.setState({delay: e.target.value})}>
                  <option value="192">192 - 4ms@48KHz</option>
                  <option value="384">384 - 8ms@48KHz</option>
                  <option value="576">576 - 12ms@48KHz</option>
                  <option value="768">768 - 16ms@48KHz</option>
                  <option value="960">960 - 20ms@48KHz</option>
                </select>
              </th>
            </tr>
            <tr height="35">
              <th align="left"> <label>Ignore RefClk GMID</label> </th>
              <th align="left"> <input type="checkbox" defaultChecked={this.state.ignoreRefclkGmid} onChange={e => this.setState({ignoreRefclkGmid: e.target.checked})} /> </th>
            </tr>
            <tr>
              <th align="left"> <label>Channels</label> </th>
              <th align="left"> <input type='number' min='1' max='64' className='input-number' value={this.state.channels} onChange={this.onChangeChannels} required/> </th>
            </tr>
            <tr>
              <th align="left">Audio Channels map</th>
              <th align="left">
                <select value={this.state.map[0]} onChange={this.onChangeChannelsMap}>
                  { this.state.channels > 1 ?
                      this.state.audioMap.map((v) => <option key={v} value={v}>{'ALSA Input ' + parseInt(v + 1, 10) + ' -> ALSA Input ' +  parseInt(v + this.state.channels, 10)}</option>) :
                      this.state.audioMap.map((v) => <option key={v} value={v}>{'ALSA Input ' + parseInt(v + 1, 10)}</option>)
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

export default SinkEdit;
