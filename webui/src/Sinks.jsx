//
//  Sinks.jsx
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

import RestAPI from './Services';
import Loader from './Loader';
import SinkEdit from './SinkEdit';
import SinkRemove from './SinkRemove';
import SinkTranscription from './SinkTranscription';

class SinkEntry extends Component {
  static propTypes = {
    id: PropTypes.number.isRequired,
    name: PropTypes.string.isRequired,
    channels: PropTypes.number.isRequired,
    onEditClick: PropTypes.func.isRequired,
    onTrashClick: PropTypes.func.isRequired,
    onTranscriptionClick: PropTypes.func.isRequired,
    transcriberEnabled: PropTypes.bool.isRequired
  };

  constructor(props) {
    super(props);
    this.state = {
      min_time: 'n/a',
      flags: 'n/a',
      errors: 'n/a'
    };
  }

  handleEditClick = () => {
    this.props.onEditClick(this.props.id);
  };

  handleTrashClick = () => {
    this.props.onTrashClick(this.props.id);
  };

  handleTranscriptionClick = () => {
    this.props.onTranscriptionClick(this.props.id);
  };

  componentDidMount() {
    RestAPI.getSinkStatus(this.props.id)
      .then(response => response.json())
      .then(function(status) {
        let errors = '';
        if (status.sink_flags.rtp_seq_id_error)
          errors += 'SEQID';
        if (status.sink_flags.rtp_ssrc_error)
          errors += (errors ? ',' : '') + 'SSRC';
        if (status.sink_flags.rtp_payload_type_error)
          errors += (errors ? ',' : '') + 'payload type';
        if (status.sink_flags.rtp_sac_error)
          errors += (errors ? ',' : '') + 'SAC';
        let flags = '';
        if (status.sink_flags.receiving_rtp_packet)
          flags += 'receiving';
        if (status.sink_flags._some_muted)
          flags += (flags ? ',' : '') + 'some muted';
        if (status.sink_flags._all_muted)
          flags += (flags ? ',' : '') + 'all muted';
        if (status.sink_flags._muted)
          flags += (flags ? ',' : '') + 'muted';
        this.setState({
          min_time: status.sink_min_time + ' ms',
          flags: flags ? flags : 'idle',
          errors: errors ? errors : 'none'
        });
      }.bind(this));
  }

  render() {
    return (
      <tr className='tr-stream'>
        <td> <label>{this.props.id}</label> </td>
        <td> <label>{this.props.name}</label> </td>
        <td align='center'> <label>{this.props.channels}</label> </td>
        <td align='center'> <label>{this.state.flags}</label> </td>
        <td align='center'> <label>{this.state.errors}</label> </td>
        <td align='center'> <label>{this.state.min_time}</label> </td>
        { this.props.transcriberEnabled ? <td> <span className='pointer-area' onClick={this.handleTranscriptionClick}> <img width='25' height='25' src='/transcription.png' alt=''/> </span> </td> : <td/> }
        <td> <span className='pointer-area' onClick={this.handleEditClick}> <img width='25' height='25' src='/edit.png' alt=''/> </span> </td>
        <td> <span className='pointer-area' onClick={this.handleTrashClick}> <img width='25' height='25' src='/trash.png' alt=''/> </span> </td>        
      </tr>
    );
  }
}


class SinkList extends Component {
  static propTypes = {
    onAddClick: PropTypes.func.isRequired,
    onReloadClick: PropTypes.func.isRequired
  };

  handleAddClick = () => {
    this.props.onAddClick();
  };

  handleReloadClick = () => {
    this.props.onReloadClick();
  };

  render() {
    return (
      <div id='sinks-table'>
        <table className="table-stream"><tbody>
          {this.props.sinks.length > 0 ?
            <tr className='tr-stream'>
              <th>ID</th>
              <th>Name</th>
              <th>Channels</th>
              <th>Status</th>
              <th>Errors</th>
              <th>Min. arrival time</th>
            </tr>
          : <tr>
             <th>No sinks configured</th>
            </tr> }
          {this.props.sinks}
        </tbody></table>
         &nbsp;
        <span className='pointer-area' onClick={this.handleReloadClick}> <img width='30' height='30' src='/reload.png' alt=''/> </span>
         &nbsp;&nbsp;
        {this.props.sinks.length < 64 ?
	  <span className='pointer-area' onClick={this.handleAddClick}> <img width='30' height='30' src='/plus.png' alt=''/> </span>
          : undefined}
      </div>
    );
  }
}

class Sinks extends Component {
  constructor(props) {
    super(props);
    this.state = {
      sinks: [],
      sink: {},
      transcriberEnabled: false,
      isLoading: false,
      isConfigLoading: false,
      isEdit: false,
      editIsOpen: false,
      removeIsOpen: false,
      transcriptionIsOpen: false,
      editTitle: ''
    };
    this.onEditClick = this.onEditClick.bind(this);
    this.onTrashClick = this.onTrashClick.bind(this);
    this.onTranscriptionClick = this.onTranscriptionClick.bind(this);
    this.onAddClick = this.onAddClick.bind(this);
    this.onReloadClick = this.onReloadClick.bind(this);
    this.openEdit = this.openEdit.bind(this);
    this.closeEdit = this.closeEdit.bind(this);
    this.openTranscription = this.openTranscription.bind(this);
    this.closeTranscription = this.closeTranscription.bind(this);
    this.applyEdit = this.applyEdit.bind(this);
    this.fetchSinks = this.fetchSinks.bind(this);
  }

  fetchSinks() {
    this.setState({isLoading: true});
    RestAPI.getSinks()
      .then(response => response.json())
      .then(
        data => this.setState( { sinks: data.sinks, isLoading: false }))
      .catch(err => this.setState( { isLoading: false } ));

    this.setState({isConfigLoading: true});
    RestAPI.getConfig()
      .then(response => response.json())
      .then(
         data => this.setState( { transcriberEnabled: data.transcriber_enabled, isConfigLoading: false }))
      .catch(err => this.setState({ isConfigLoading: false }));

  }

  componentDidMount() {
    this.fetchSinks();
  }

  openEdit(title, sink, isEdit) {
    this.setState({editIsOpen: true, editTitle: title, sink: sink, isEdit: isEdit});
  }

  openTranscription(title, sink, isEdit) {
    this.setState({transcriptionIsOpen: true, transcriptionTitle: title, sink: sink});
  }

  applyEdit() {
    this.closeEdit();
    this.fetchSinks();
  }

  onReloadClick() {
    this.fetchSinks();
  }

  closeEdit() {
    this.setState({editIsOpen: false});
    this.setState({removeIsOpen: false});
    this.setState({transcriptionIsOpen: false});
    this.fetchSinks();
  }

  closeTranscription() {
    this.setState({IsOpen: false});
    this.setState({removeIsOpen: false});
    this.setState({transcriptionIsOpen: false});
    this.fetchSinks();
  }  

  onEditClick(id) {
    const sink = this.state.sinks.find(s => s.id === id);
    this.openEdit("Edit Sink " + id, sink, true);
  }

  onTrashClick(id) {
    const sink = this.state.sinks.find(s => s.id === id);
    this.setState({removeIsOpen: true, sink: sink});
  }

  onTranscriptionClick(id) {
    const sink = this.state.sinks.find(s => s.id === id);
    this.openTranscription("Transcription Sink " + id, sink);
  }  

  onAddClick() {
    let id;
    /* find first free id */
    for (id = 0; id < 63; id++) {
      if (this.state.sinks[id] === undefined ||
          this.state.sinks[id].id !== id) {
        break;
      }
    }
    const defaultSink = {
      'id': id,
      'name': 'ALSA Sink ' + id,
      'io': 'Audio Device',
      'delay': 576,
      'use_sdp': false,
      'source': RestAPI.getBaseUrl() + '/api/source/sdp/' + id,
      'sdp': '',
      'ignore_refclk_gmid': true,
      'map': [ (id * 2) % 64, (id * 2 + 1) % 64 ]
    };
    this.openEdit('Add Sink ' + id, defaultSink, false);
  }

  render() {
    this.state.sinks.sort((a, b) => (a.id > b.id) ? 1 : -1);
    const sinks = this.state.sinks.map((sink) => (
      <SinkEntry key={sink.id}
        id={sink.id}
        channels={sink.map.length}
        name={sink.name}
        onEditClick={this.onEditClick}
        onTrashClick={this.onTrashClick}
        onTranscriptionClick={this.onTranscriptionClick}
        transcriberEnabled={this.state.transcriberEnabled}
      />
    ));
    return (
      <div id='sinks'>
       { this.state.isLoading || this.state.isConfigLoading ? <Loader/>
	   : <SinkList onAddClick={this.onAddClick}
               onReloadClick={this.onReloadClick}
               sinks={sinks} /> }
       { this.state.editIsOpen ?
        <SinkEdit editIsOpen={this.state.editIsOpen}
          closeEdit={this.closeEdit}
          applyEdit={this.applyEdit}
          editTitle={this.state.editTitle}
	        isEdit={this.state.isEdit}
	        sink={this.state.sink} />
           : undefined }
       { this.state.removeIsOpen ?
        <SinkRemove removeIsOpen={this.state.removeIsOpen}
          closeEdit={this.closeEdit}
          applyEdit={this.applyEdit}
	        sink={this.state.sink}
	        key={this.state.sink.id} />
           : undefined }
       { this.state.transcriptionIsOpen ?
        <SinkTranscription transcriptionIsOpen={this.state.transcriptionIsOpen}
          closeTranscription={this.closeTranscription}
          transcriptionTitle={this.state.transcriptionTitle}
	        sink={this.state.sink} />
           : undefined }           
      </div>
    );
  }
}

export default Sinks;
