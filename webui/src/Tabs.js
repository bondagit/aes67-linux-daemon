//
//  Tabs.js
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

import React, { Component } from 'react';
import PropTypes from 'prop-types';

import Tab from './Tab';

class Tabs extends Component {
  static propTypes = {
    children: PropTypes.instanceOf(Array).isRequired,
    currentTab: PropTypes.string.isRequired,
  }

  constructor(props) {
    super(props);
    this.state = { activeTab: this.props.currentTab, };
  }

  onClickTabItem = (tab) => {
    this.setState({ activeTab: tab });
    window.history.pushState(tab, tab, '/' + tab);
  }

  render() {
    const {
      onClickTabItem,
      props: { children, },
      state: { activeTab, }
    } = this;

    return ( 
      <div className="tabs">
        <ol className="tab-list"> { children.map((child) => { 
          const { label } = child.props;
          return ( 
            <Tab activeTab={ activeTab }
                 key={ label }
                 label={ label }
                 onClick={ onClickTabItem }
            />
          ); })
        }
        </ol> 
        <div className="tab-content"> { children.map((child) => {
          if (child.props.label !== activeTab) return undefined;
          return child.props.children;
        }) } 
        </div>
      </div>
    );
  }
}

export default Tabs;
