//
//  SinkTranscription.jsx
//
//  Copyright (c) 2019 2025 Andrea Bondavalli. All rights reserved.
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

const trnacriptionCustomStyles = {
  content : {
    top:  '50%',
    left: '50%',
    right: 'auto',
    bottom: 'auto',
    marginRight: '-50%',
    transform: 'translate(-50%, -50%)'
  }
};

class SinkTranscription extends Component {
  static propTypes = {
    sink: PropTypes.object.isRequired,
    closeTranscription: PropTypes.func.isRequired,
    TranscriptionIsOpen: PropTypes.bool.isRequired,
    TranscriptionTitle: PropTypes.string.isRequired
  };

  constructor(props) {
    super(props);
    this.state = {
      transcription: "",
      id: this.props.sink.id,
      name: this.props.sink.name,
      channels: this.props.sink.map.length,
    }
    this.onClose = this.onClose.bind(this);
    this.onClear = this.onClear.bind(this);
    this.fetchTranscription = this.fetchTranscription.bind(this);
  }

  fetchTranscription() {
    RestAPI.getTranscription(this.state.id)
      .then(response => response.text())
      .then(
        text => this.setState( { transcription: text }))
  }

  componentDidMount() {
    Modal.setAppElement('body');
    this.fetchTranscription();
    this.interval = setInterval(() => { this.fetchTranscription() }, 5000)
  }

  componentWillUnmount() {
    clearInterval(this.interval);
  }  

  onClear() {
    RestAPI.clearTranscription(this.state.id).then(
      this.setState( { transcription: "" })
    )
  }

  onClose() {
    this.props.closeTranscription();
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

  render()  {
    return (
      <div id='sink-transcription'>
        <Modal ariaHideApp={false}
          isOpen={this.props.transcriptionIsOpen}
          onRequestClose={this.props.closeTranscription}
          style={trnacriptionCustomStyles}
          contentLabel="Sink Transcription">
          <h2><center>{this.props.transcriptionTitle}</center></h2>
          <table><tbody>
            <tr>
              <th align="left"> <label>ID</label> </th>
              <th align="left"> <input value={this.state.id} readOnly/> </th>
            </tr>
            <tr>
              <th align="left"> <label>Name</label> </th>
              <th align="left"> <input value={this.state.name} readOnly/> </th>
            </tr>
            <tr>
              <th align="left"> <label>Channels</label> </th>
              <th align="left"> <input value={this.state.channels} readOnly/> </th>
            </tr>
            <tr>
              <th align="left"> <label>Text</label> </th>
              <th align="left"> <textarea rows='25' cols='80' value={this.state.transcription} readOnly/> </th>
            </tr>            
          </tbody></table>
          <br/>
	        <div style={{textAlign: 'center'}}>
            <button onClick={this.onClear}>Clear</button>
            &nbsp;&nbsp;&nbsp;&nbsp;
            <button onClick={this.onClose}>Close</button>
          </div>
        </Modal>
      </div>
    );
  }
}

export default SinkTranscription;
