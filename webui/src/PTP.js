//
//  PTP.js
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
import {toast} from 'react-toastify';

import RestAPI from './Services';
import Loader from './Loader';

require('./styles.css');

class PTP extends Component {
  constructor(props) {
    super(props);
    this.state = {
      domain: '',
      domainErr: false,
      dscp: '',
      status: '',
      gmid: '',
      jitter: '',
      isConfigLoading: false,
      isStatusLoading: false,
    };
    this.onSubmit = this.onSubmit.bind(this);
    this.inputIsValid = this.inputIsValid.bind(this);
  }

  fetchStatus() {
    this.setState({isStatusLoading: true});
    RestAPI.getPTPStatus()
      .then(response => response.json())
      .then(
        data => this.setState({
           status: data.status, 
           gmid: data.gmid, 
           jitter: data.jitter, 
           isStatusLoading: false
        }))
      .catch(err => this.setState({isStatusLoading: false}));
  }
  
  componentDidMount() {
    this.setState({isConfigLoading: true});
    RestAPI.getPTPConfig()
      .then(response => response.json())
      .then(data =>
        this.setState({domain: data.domain, dscp: data.dscp, isConfigLoading: false}))
      .catch(err => this.setState({isConfigLoading: false}));

    this.fetchStatus();
    this.interval = setInterval(() => { this.fetchStatus() }, 10000)
  }
  
  componentWillUnmount() {
    clearInterval(this.interval);
  }

  inputIsValid() {
    return !this.state.domainErr &&
      !this.state.isConfigLoading;
  }

  onSubmit(event) {
    event.preventDefault();
    RestAPI.setPTPConfig(this.state.domain, this.state.dscp)
      .then(response => toast.success('PTP config updated'));
  }
  
  render() {
    return (
      <div className='ptp'>
        {this.state.isConfigLoading ? <Loader/> : <h3>Config</h3>}
        <table><tbody>
          <tr>
            <th align="left"> <label>Type</label> </th>
            <th align="left"> <label>PTPv2</label> </th>
          </tr>
          <tr>
            <th align="left"> <label>Domain</label> </th>
             <th align="left"> <input type='number' min='0' max='127' className='input-number' value={this.state.domain} onChange={e => this.setState({domain: e.target.value, domainErr: !e.currentTarget.checkValidity()})} required/> </th>
          </tr>
          <tr>
            <th align="left"> <label>DSCP</label> </th>
            <th align="left">
              <select value={this.state.dscp} onChange={e => this.setState({dscp: e.target.value})}>
                <option value='46'>46 (EF)</option>
                <option value='48'>48 (CS6)</option>
              </select>
            </th>
          </tr>
          <tr>
            <th> <button disabled={this.inputIsValid() ? undefined : true} onClick={this.onSubmit}>Submit</button> </th>
          </tr>
        </tbody></table>
        <br/>
        { this.state.isStatusLoading ? <Loader/> : <h3>Status</h3> }
        <table><tbody>
          <tr>
            <th align="left"> <label>Mode</label> </th>
            <th align="left"> <input value='Slave' disabled/> </th>
          </tr>
          <tr>
            <th align="left"> <label>Status</label> </th>
            <th align="left"> <input value={this.state.status} disabled/> </th>
          </tr>
          <tr>
            <th align="left"> <label>GMID</label> </th>
            <th align="left"> <input value={this.state.gmid} disabled/> </th>
          </tr>
          <tr>
            <th align="left"> <label>Delta</label> </th>
            <th align="left"> <input value={this.state.jitter} disabled/> </th>
          </tr>
        </tbody></table>
      </div>
    )
  }
}

export default PTP;
