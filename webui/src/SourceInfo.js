//
//  SourceInfo.js
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
import Modal from 'react-modal';

require('./styles.css');

const infoCustomStyles = {
  content : {
    top:  '50%',
    left: '50%',
    right: 'auto',
    bottom: 'auto',
    marginRight: '-50%',
    transform: 'translate(-50%, -50%)'
  }
};

class SourceInfo extends Component {
  static propTypes = {
    id: PropTypes.string.isRequired,
    name: PropTypes.string.isRequired,
    sdp: PropTypes.string.isRequired,
    closeInfo: PropTypes.func.isRequired,
    infoIsOpen: PropTypes.bool.isRequired,
    infoTitle: PropTypes.string.isRequired
  };

  constructor(props) {
    super(props);
    this.onClose = this.onClose.bind(this);
  }

  componentDidMount() {
    Modal.setAppElement('body');
  }

  onClose() {
    this.props.closeInfo();
  }
 
  render()  {
    return (
      <div id='source-info'>
        <Modal ariaHideApp={false} 
          isOpen={this.props.infoIsOpen}
          onRequestClose={this.props.closeInfo}
          style={infoCustomStyles}
          contentLabel="Srouce SDP Info">
          <h2><center>{this.props.infoTitle}</center></h2>
          <table><tbody>
            <tr>
              <th align="left"> <label>ID</label> </th>
              <th align="left"> <input value={this.props.id} readOnly/> </th>
            </tr>
            <tr>
              <th align="left"> <label>Name</label> </th>
              <th align="left"> <input value={this.props.name} readOnly/> </th>
            </tr>
            <tr>
              <th align="left"> <label>SDP</label> </th>
              <th align="left"> <textarea rows='15' cols='55' value={this.props.sdp} readOnly/> </th>
            </tr>
          </tbody></table>
          <br/>
	  <div style={{textAlign: 'center'}}>
            <button onClick={this.onClose}>Close</button>
          </div>
        </Modal>
      </div>
    );
  }
}

export default SourceInfo;
