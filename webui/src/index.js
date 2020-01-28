//
//  index.js
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
//
//
import React from 'react'
import { Route, BrowserRouter as Router, Switch } from 'react-router-dom'
import { render } from "react-dom";
import { toast } from 'react-toastify';
import 'react-toastify/dist/ReactToastify.css';

import ConfigTabs from './ConfigTabs';

toast.configure()

function App() {
  return ( <div>
  <Router>
    <div>
      <Switch>
        <Route exact path='/PTP' component={() => <ConfigTabs key='PTP' currentTab='PTP' />} />
        <Route exact path='/Sources' component={() => <ConfigTabs key='Sources' currentTab='Sources' />} />
        <Route exact path='/Sinks' component={() => <ConfigTabs key='Sinks' currentTab='Sinks' />} />
        <Route component={() => <ConfigTabs key='Config' currentTab='Config' />} />
      </Switch>
    </div>
  </Router>
   </div>
  );
}

const container = document.createElement('div');
document.body.appendChild(container);
render( <App/>, container);
