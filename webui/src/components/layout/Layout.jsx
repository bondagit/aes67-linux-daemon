import React, { useState, useEffect } from 'react';
import { Outlet, useLocation } from 'react-router-dom';
import Sidebar from './Sidebar';

export default function Layout() {
  const location = useLocation();
  const [collapsed, setCollapsed] = useState(false);

  // Auto-collapse sidebar on the matrix route
  useEffect(() => {
    if (location.pathname === '/matrix') {
      setCollapsed(true);
    }
  }, [location.pathname]);

  return (
    <div style={{ display: 'flex', minHeight: '100vh' }}>
      <Sidebar collapsed={collapsed} onToggle={() => setCollapsed((c) => !c)} />
      <main
        style={{
          flex: 1,
          overflow: 'auto',
          minWidth: 0,
        }}
      >
        <Outlet />
      </main>
    </div>
  );
}
