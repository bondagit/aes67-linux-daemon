import React from 'react';
import { Routes, Route, Navigate } from 'react-router-dom';
import Layout from './components/layout/Layout';
import Dashboard from './views/Dashboard/Dashboard';
import Matrix from './views/Matrix/Matrix';
import Sources from './views/Sources/Sources';
import Sinks from './views/Sinks/Sinks';
import Browser from './views/Browser/Browser';
import Settings from './views/Settings/Settings';
import Monitoring from './views/Monitoring/Monitoring';

export default function App() {
  return (
    <Routes>
      <Route element={<Layout />}>
        <Route index element={<Dashboard />} />
        <Route path="monitoring" element={<Monitoring />} />
        <Route path="matrix" element={<Matrix />} />
        <Route path="sources" element={<Sources />} />
        <Route path="sinks" element={<Sinks />} />
        <Route path="browser" element={<Browser />} />
        <Route path="settings" element={<Settings />} />
        <Route path="*" element={<Navigate to="/" replace />} />
      </Route>
    </Routes>
  );
}
