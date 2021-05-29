//
//  SourceRemove.jsx
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

const removeCustomStyles = {
  content : {
    top:  '50%',
    left: '50%',
    right: 'auto',
    bottom: 'auto',
    marginRight: '-50%',
    transform: 'translate(-50%, -50%)'
  }
};

class SourceRemove extends Component {
  static propTypes = {
    source: PropTypes.object.isRequired,
    applyEdit: PropTypes.func.isRequired,
    closeEdit: PropTypes.func.isRequired,
    removeIsOpen: PropTypes.bool.isRequired
  };

  constructor(props) {
    super(props);
    this.onSubmit = this.onSubmit.bind(this);
    this.onCancel = this.onCancel.bind(this);
    this.removeSource = this.removeSource.bind(this);
  }

  componentDidMount() {
    Modal.setAppElement('body');
  }

  removeSource(message) {
    RestAPI.removeSource(this.props.source.id).then(function(response) {
      toast.success(message);
      this.props.applyEdit();
    }.bind(this));
  }

  onSubmit() {
   this.removeSource('Source ' + this.props.source.id + ' removed');
  }

  onCancel() {
    this.props.closeEdit();
  }

  render()  {
    return (
      <div id='source-remove'>
        <Modal ariaHideApp={false}
          isOpen={this.props.removeIsOpen}
          onRequestClose={this.props.closeEdit}
          style={removeCustomStyles}
          contentLabel="Source Remove">
          <h2><center>{'Remove source ' + this.props.source.id}</center></h2>
	  <h3><center>Do you want to proceed ?</center></h3>
          <br/>
	  <div style={{textAlign: 'center'}}>
            <button onClick={this.onSubmit}>Submit</button>
            &nbsp;&nbsp;&nbsp;&nbsp;
            <button onClick={this.onCancel}>Cancel</button>
          </div>
        </Modal>
      </div>
    );
  }
}

export default SourceRemove;
