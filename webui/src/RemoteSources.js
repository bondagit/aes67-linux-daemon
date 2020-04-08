//
//  RemoteSource.js
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
import SourceInfo from './SourceInfo';

require('./styles.css');

class RemoteSourceEntry extends Component {
  static propTypes = {
    source: PropTypes.string.isRequired,
    id: PropTypes.string.isRequired,
    name: PropTypes.string.isRequired,
    domain: PropTypes.string.isRequired,
    address: PropTypes.string.isRequired,
    sdp: PropTypes.string.isRequired,
    last_seen: PropTypes.number.isRequired,
    period: PropTypes.number.isRequired,
    onInfoClick: PropTypes.func.isRequired,
  };

  constructor(props) {
    super(props);
    this.state = { 
      rtp_address: 'n/a',
      port: 'n/a' 
    };
  }

  handleInfoClick = () => {
    this.props.onInfoClick(this.props.id);
  };

  componentDidMount() {
    var rtp_address = this.props.sdp.match(/(c=IN IP4 )([0-9.]+)/g);
    var port = this.props.sdp.match(/(m=audio )([0-9]+)/g);
    if (rtp_address && port) {
      this.setState({ rtp_address: rtp_address[0].substr(9), port: port[0].substr(8) });
    }
  }

  render() {
    return (
      <tr className='tr-browser'>
        <td> <label>{this.props.source}</label> </td>
        <td> <label>{this.props.address}</label> </td>
        <td> <label>{this.props.name}</label> </td>
        <td> <label>{this.props.source=='mDNS' ? this.props.domain : 'N/A'}</label> </td>
        <td> <label>{this.state.rtp_address}</label> </td>
        <td align='center'> <label>{this.state.port}</label> </td>
        <td align='center'> <label>{this.props.last_seen}</label> </td>
        <td align='center'> <label>{this.props.source=='SAP' ? this.props.period : 'N/A'}</label> </td>
        <td> <span className='pointer-area' onClick={this.handleInfoClick}> <img width='20' height='20' src='/info.png' alt=''/> </span> </td>
      </tr>
    );
  }
}


class RemoteSourceList extends Component {
  static propTypes = {
    onReloadClick: PropTypes.func.isRequired
  };

  handleReloadClick = () => {
    this.props.onReloadClick();
  };

  render() {
    return (
      <div id='remote-sources-table'>
        <table className="table-stream"><tbody>
          {this.props.sources.length > 0 ? 
            <tr className='tr-stream'>
              <th>Source</th>
              <th>Address</th>
              <th>Name</th>
              <th>Domain</th>
              <th>RTP Address</th>
              <th>Port</th>
              <th>Seen</th>
              <th>Period</th>
            </tr>
          : <tr>
             <th>No remote sources found.</th>
            </tr> }
          {this.props.sources}
        </tbody></table>
         &nbsp;
        <span className='pointer-area' onClick={this.handleReloadClick}> <img width='30' height='30' src='/reload.png' alt=''/> </span>
         &nbsp;&nbsp;
      </div>
    );
  }
}

class RemoteSources extends Component {
  constructor(props) {
    super(props);
    this.state = { 
      sources: [], 
      isLoading: false, 
      infoIsOpen: false, 
      infoTitle: '' 
    };
    this.onInfoClick = this.onInfoClick.bind(this);
    this.onReloadClick = this.onReloadClick.bind(this);
    this.openInfo = this.openInfo.bind(this);
    this.closeInfo = this.closeInfo.bind(this);
    this.fetchRemoteSources = this.fetchRemoteSources.bind(this);
  }

  fetchRemoteSources() {
    this.setState({isLoading: true});
    RestAPI.getRemoteSources()
      .then(response => response.json())
      .then(
        data => this.setState( { sources: data.remote_sources, isLoading: false }))
      .catch(err => this.setState( { isLoading: false } ));
  }

  componentDidMount() {
    this.fetchRemoteSources();
  }

  openInfo(title, source) {
    this.setState({infoIsOpen: true, infoTitle: title, source: source});
  }

  closeInfo() {
    this.setState({infoIsOpen: false});
    this.fetchRemoteSources();
  }
  
  onInfoClick(id) {
    const source = this.state.sources.find(s => s.id === id);
    this.openInfo("Announced Source Info", source, true);
  }

  onReloadClick() {
    this.fetchRemoteSources();
  }

  render() {
    //this.state.sources.sort((a, b) => (a.id > b.id) ? 1 : -1);
    const sources = this.state.sources.map((source) => (
      <RemoteSourceEntry key={source.id}
        source={source.source}
        id={source.id}
        address={source.address}
        name={source.name}
        domain={source.domain}
        sdp={source.sdp}
        last_seen={source.last_seen}
        period={source.announce_period}
        onInfoClick={this.onInfoClick}
      />
    ));
    return (
      <div id='sources'>
       { this.state.isLoading ? <Loader/> 
	   : <RemoteSourceList
               onReloadClick={this.onReloadClick}
               sources={sources} /> }
       { this.state.infoIsOpen ?
        <SourceInfo infoIsOpen={this.state.infoIsOpen}
          closeInfo={this.closeInfo}
          infoTitle={this.state.infoTitle} 
	  id={this.state.source.id}
	  source={this.state.source.source}
	  name={this.state.source.name}
	  domain={this.state.source.domain}
	  sdp={this.state.source.sdp} />
           : undefined }
      </div>
    );
  }
}

export default RemoteSources;
