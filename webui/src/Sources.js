//
//  Sources.js
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

import RestAPI from './Services';
import Loader from './Loader';
import SourceEdit from './SourceEdit';
import SourceRemove from './SourceRemove';
import SourceInfo from './SourceInfo';

require('./styles.css');

class SourceEntry extends Component {
  static propTypes = {
    id: PropTypes.number.isRequired,
    name: PropTypes.string.isRequired,
    channels: PropTypes.number.isRequired,
    onEditClick: PropTypes.func.isRequired,
    onInfoClick: PropTypes.func.isRequired,
    onTrashClick: PropTypes.func.isRequired
  };

  constructor(props) {
    super(props);
    this.state = { 
      address: 'n/a',
      port: 'n/a',
      sdp: '' 
    };
  }

  handleInfoClick = () => {
    this.props.onInfoClick(this.props.id, this.state.sdp);
  };

  handleEditClick = () => {
    this.props.onEditClick(this.props.id);
  };

  handleTrashClick = () => {
    this.props.onTrashClick(this.props.id);
  };

  componentDidMount() {
    RestAPI.getSourceSDP(this.props.id)
      .then(response => response.text())
      .then(function(sdp) {
        var address = sdp.match(/(c=IN IP4 )([0-9.]+)/g);
        var port = sdp.match(/(m=audio )([0-9]+)/g);
        this.setState({ sdp: sdp });
        if (address && port) {
          this.setState({ address: address[0].substr(9), port: port[0].substr(8) });
        }
      }.bind(this));
  }

  render() {
    return (
      <tr className='tr-stream'>
        <td> <label>{this.props.id}</label> </td>
        <td> <label>{this.props.name}</label> </td>
        <td> <label>{this.state.address}</label> </td>
        <td> <label>{this.state.port}</label> </td>
        <td align='center'> <label>{this.props.channels}</label> </td>
        <td> <span className='pointer-area' onClick={this.handleInfoClick}> <img width='20' height='20' src='/info.png' alt=''/> </span> </td>
        <td> <span className='pointer-area' onClick={this.handleEditClick}> <img width='20' height='20' src='/edit.png' alt=''/> </span> </td>
        <td> <span className='pointer-area' onClick={this.handleTrashClick}> <img width='20' height='20' src='/trash.png' alt=''/> </span> </td>
      </tr>
    );
  }
}


class SourceList extends Component {
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
      <div id='sources-table'>
        <table className="table-stream"><tbody>
          {this.props.sources.length > 0 ? 
            <tr className='tr-stream'>
              <th>ID</th>
              <th>Name</th>
              <th>Address</th>
              <th>Port</th>
              <th>Channels</th>
            </tr>
          : <tr>
             <th>No sources configured</th>
            </tr> }
          {this.props.sources}
        </tbody></table>
         &nbsp;
        <span className='pointer-area' onClick={this.handleReloadClick}> <img width='30' height='30' src='/reload.png' alt=''/> </span>
         &nbsp;&nbsp;
        {this.props.sources.length < 64 ?
	  <span className='pointer-area' onClick={this.handleAddClick}> <img width='30' height='30' src='/plus.png' alt=''/> </span> 
          : undefined}
      </div>
    );
  }
}

class Sources extends Component {
  constructor(props) {
    super(props);
    this.state = { 
      sources: [], 
      source: {}, 
      isLoading: false, 
      isEdit: false, 
      isInfo: false, 
      editIsOpen: false, 
      infoIsOpen: false, 
      removeIsOpen: false, 
      editTitle: '' 
    };
    this.onInfoClick = this.onInfoClick.bind(this);
    this.onEditClick = this.onEditClick.bind(this);
    this.onTrashClick = this.onTrashClick.bind(this);
    this.onAddClick = this.onAddClick.bind(this);
    this.onReloadClick = this.onReloadClick.bind(this);
    this.openInfo = this.openInfo.bind(this);
    this.openEdit = this.openEdit.bind(this);
    this.closeEdit = this.closeEdit.bind(this);
    this.closeInfo = this.closeInfo.bind(this);
    this.applyEdit = this.applyEdit.bind(this);
    this.fetchSources = this.fetchSources.bind(this);
  }

  fetchSources() {
    this.setState({isLoading: true});
    RestAPI.getSources()
      .then(response => response.json())
      .then(
        data => this.setState( { sources: data.sources, isLoading: false }))
      .catch(err => this.setState( { isLoading: false } ));
  }

  componentDidMount() {
    this.fetchSources();
  }

  openInfo(title, source, sdp, isInfo) {
    this.setState({infoIsOpen: true, infoTitle: title, source: source, sdp: sdp, isInfo: isInfo});
  }

  openEdit(title, source, isEdit) {
    this.setState({editIsOpen: true, editTitle: title, source: source, isEdit: isEdit});
  }

  applyEdit() {
    this.closeEdit();
    this.fetchSources();
  }
 
  closeEdit() {
    this.setState({editIsOpen: false});
    this.setState({removeIsOpen: false});
    this.fetchSources();
  }

  closeInfo() {
    this.setState({infoIsOpen: false});
  }
  
  onInfoClick(id, sdp) {
    const source = this.state.sources.find(s => s.id === id);
    this.openInfo("Local Source Info", source, sdp, true);
  }

  onEditClick(id) {
    const source = this.state.sources.find(s => s.id === id);
    this.openEdit("Edit Source " + id, source, true);
  }

  onTrashClick(id) {
    const source = this.state.sources.find(s => s.id === id);
    this.setState({removeIsOpen: true, source: source});
  }

  onReloadClick() {
    this.fetchSources();
  }

  onAddClick() {
    let id; 
    /* find first free id */
    for (id = 0; id < 63; id++) {
      if (this.state.sources[id] === undefined || 
          this.state.sources[id].id !== id) {
        break;
      }
    }
    const defaultSource = { 
      'id': id,
      'enabled': true,
      'name': 'ALSA Source ' + id,
      'io': 'Audio Device',
      'max_samples_per_packet': 48,
      'codec': 'L16',
      'ttl': 15,
      'payload_type': 98,
      'dscp': 34,
      'refclk_ptp_traceable': false,
      'map': [ (id * 2) % 64, (id * 2 + 1) % 64 ]
    };
    this.openEdit('Add Source ' + id, defaultSource, false);
  }
  
  render() {
    this.state.sources.sort((a, b) => (a.id > b.id) ? 1 : -1);
    const sources = this.state.sources.map((source) => (
      <SourceEntry key={source.id}
        id={source.id}
        name={source.name}
        channels={source.map.length}
        onInfoClick={this.onInfoClick}
        onEditClick={this.onEditClick}
        onTrashClick={this.onTrashClick}
      />
    ));
    return (
      <div id='sources'>
       { this.state.isLoading ? <Loader/> 
	   : <SourceList onAddClick={this.onAddClick} 
               onReloadClick={this.onReloadClick}
               sources={sources} /> }
       { this.state.infoIsOpen ?
        <SourceInfo infoIsOpen={this.state.infoIsOpen}
          closeInfo={this.closeInfo}
          infoTitle={this.state.infoTitle} 
	  isInfo={this.state.isInfo}
	  id={this.state.source.id.toString()}
	  name={this.state.source.name}
	  source='local'
	  sdp={this.state.sdp} />
           : undefined }
       { this.state.editIsOpen ?
        <SourceEdit editIsOpen={this.state.editIsOpen}
          closeEdit={this.closeEdit}
          applyEdit={this.applyEdit}
          editTitle={this.state.editTitle} 
	  isEdit={this.state.isEdit}
	  source={this.state.source} />
           : undefined }
       { this.state.removeIsOpen ?
        <SourceRemove removeIsOpen={this.state.removeIsOpen}
          closeEdit={this.closeEdit}
          applyEdit={this.applyEdit}
	  source={this.state.source} 
	  key={this.state.source.id} />
           : undefined }
      </div>
    );
  }
}

export default Sources;
