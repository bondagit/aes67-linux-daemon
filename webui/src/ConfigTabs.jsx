//
//  ConfigTabs.jsx
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

import Tabs from './Tabs';
import PTP from './PTP';
import Config from './Config';
import Sources from './Sources';
import Sinks from './Sinks';
import RemoteSources from './RemoteSources';

class ConfigTabs extends Component {
  static propTypes = {
    currentTab: PropTypes.string.isRequired
  };

  constructor(props) {
    super(props);
    this.state = {
      titleExtras: ''
    };
    this.setTitleExtras = this.setTitleExtras.bind(this);
  }

  setTitleExtras(extras) {
    if (this.state.titleExtras != extras)
        this.setState({titleExtras: extras});
  }

  render() {
    return (
      <div>
        <h1>AES67 Daemon</h1>
        <h3>Running on {this.state.titleExtras}</h3>
        <Tabs currentTab={this.props.currentTab}>
          <div label="Config">
            <Config setTitleExtras={this.setTitleExtras} />
          </div>
          <div label="PTP">
            <PTP/>
          </div>
          <div label="Sources">
            <Sources/>
          </div>
          <div label="Sinks">
            <Sinks/>
          </div>
          <div label="Browser">
            <RemoteSources/>
          </div>
        </Tabs>
       </div>
    );
  }
}

export default ConfigTabs;
